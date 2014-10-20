/*
 * Copyright   Actility, SA. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 only, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is
 * included at /legal/license.txt).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * Please contact Actility, SA.,  4, rue Ampere 22300 LANNION FRANCE
 * or visit www.actility.com if you need additional
 * information or have any questions.
 *
 * id $Id: M2MPocs.java 4140 2013-01-15 14:08:57Z JReich $
 * author $Author: JReich $
 * version $Revision: 4140 $
 * lastrevision $Date: 2013-01-15 15:08:57 +0100 (Tue, 15 Jan 2013) $
 * modifiedby $LastChangedBy: JReich $
 * lastmodified $LastChangedDate: 2013-01-15 15:08:57 +0100 (Tue, 15 Jan 2013) $
 */

package com.actility.m2m.scl.res;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Date;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

import org.apache.log4j.Logger;

import com.actility.m2m.m2m.FilterCriteria;
import com.actility.m2m.m2m.Indication;
import com.actility.m2m.m2m.M2MConstants;
import com.actility.m2m.m2m.M2MException;
import com.actility.m2m.m2m.StatusCode;
import com.actility.m2m.scl.log.BundleLogger;
import com.actility.m2m.scl.model.Constants;
import com.actility.m2m.scl.model.SclLogger;
import com.actility.m2m.scl.model.SclManager;
import com.actility.m2m.scl.model.SclTransaction;
import com.actility.m2m.storage.Condition;
import com.actility.m2m.storage.ConditionBuilder;
import com.actility.m2m.storage.SearchResult;
import com.actility.m2m.storage.StorageException;
import com.actility.m2m.storage.StorageRequestExecutor;
import com.actility.m2m.util.FormatUtils;
import com.actility.m2m.util.Pair;
import com.actility.m2m.util.Profiler;
import com.actility.m2m.util.URIUtils;
import com.actility.m2m.util.UUID;
import com.actility.m2m.util.log.OSGiLogger;
import com.actility.m2m.xo.XoException;
import com.actility.m2m.xo.XoObject;

public final class M2MPocs extends SclResource {
    private static final Logger LOG = OSGiLogger.getLogger(M2MPocs.class, BundleLogger.getStaticLogger());

    private static final M2MPocs INSTANCE = new M2MPocs();

    public static M2MPocs getInstance() {
        return INSTANCE;
    }

    private final Set reservedKeywords = new HashSet();

    private M2MPocs() {
        // Cannot be instantiated
        super(true, "m2mPocRetrieveRequestIndication", "m2mPocRetrieveResponseConfirm", null, null, null, null,
                Constants.ID_NO_FILTER_CRITERIA_MODE, true, false, null, false, false);
        reservedKeywords.add(M2MConstants.ATTR_CREATION_TIME);
        reservedKeywords.add(M2MConstants.ATTR_LAST_MODIFIED_TIME);
        reservedKeywords.add(M2MConstants.ATTR_M2M_POC_COLLECTION);
    }

    // m2m:M2MPocs from ong:t_xml_obj
    // {
    // m2m:creationTime XoString { } // (optional) (xmlType: xsd:dateTime)
    // m2m:lastModifiedTime XoString { } // (optional) (xmlType: xsd:dateTime)
    // m2m:m2mPocCollection m2m:NamedReferenceCollection { } // (optional)
    // }
    // alias m2m:M2MPocs with m2m:m2mPocs

    public void checkRights(String logId, SclManager manager, String path, XoObject resource, URI requestingEntity, String flag)
            throws M2MException {
        checkDefaultAccessRight(logId, manager, path, requestingEntity);
    }

    public void createResource(SclManager manager, String path, Date creationDate, String creationTime,
            SclTransaction transaction) throws XoException {

        XoObject resource = null;
        try {
            resource = manager.getXoService().newXmlXoObject(M2MConstants.TAG_M2M_M2M_POCS);

            // Update resource
            resource.setNameSpace(M2MConstants.PREFIX_M2M);
            resource.setStringAttribute(M2MConstants.TAG_M2M_CREATION_TIME, creationTime);
            resource.setStringAttribute(M2MConstants.TAG_M2M_LAST_MODIFIED_TIME, creationTime);

            // Save resource
            Collection searchAttributes = new ArrayList();
            searchAttributes.add(new Pair(M2MConstants.ATTR_CREATION_TIME, creationDate));
            searchAttributes.add(new Pair(M2MConstants.ATTR_LAST_MODIFIED_TIME, creationDate));
            transaction.createResource(path, resource, searchAttributes);
        } finally {
            if (resource != null) {
                resource.free(true);
            }
        }
    }

    public boolean updateResource(String logId, SclManager manager, String path, XoObject resource, Indication indication,
            XoObject representation, Date now) {
        throw new UnsupportedOperationException();
    }

    public void deleteResource(String logId, SclManager manager, String path, SclTransaction transaction)
            throws UnsupportedEncodingException, StorageException, XoException, M2MException {
        if (LOG.isInfoEnabled()) {
            LOG.info("Delete <m2mPocs> resource on path: " + path);
        }
        SearchResult searchResult = manager.getStorageContext().search(path, StorageRequestExecutor.SCOPE_EXACT, null);
        Map children = searchResult.getResults();
        Iterator it = children.entrySet().iterator();
        Entry entry = null;
        String subPath = null;
        M2MPoc m2mPoc = M2MPoc.getInstance();
        while (it.hasNext()) {
            entry = (Entry) it.next();
            subPath = (String) entry.getKey();
            if (manager.getDefaultResourceId(subPath) == Constants.ID_RES_M2M_POC) {
                m2mPoc.deleteResource(logId, manager, subPath, transaction);
            }
        }

        transaction.deleteResource(path);
    }

    public void deleteResource(String logId, SclManager manager, String path, XoObject resource, SclTransaction transaction)
            throws UnsupportedEncodingException, StorageException, XoException, M2MException {
        deleteResource(logId, manager, path, transaction);
    }

    public void deleteChildResource(String logId, SclManager manager, String path, XoObject resource, XoObject childResource,
            Date now, SclTransaction transaction) throws ParseException, XoException {
        resource.setStringAttribute(M2MConstants.TAG_M2M_LAST_MODIFIED_TIME,
                FormatUtils.formatDateTime(now, manager.getTimeZone()));

        Collection searchAttributes = new ArrayList();
        searchAttributes.add(new Pair(M2MConstants.ATTR_CREATION_TIME, FormatUtils.parseDateTime(resource
                .getStringAttribute(M2MConstants.TAG_M2M_CREATION_TIME))));
        searchAttributes.add(new Pair(M2MConstants.ATTR_LAST_MODIFIED_TIME, now));
        transaction.updateResource(path, resource, searchAttributes);
    }

    public void prepareResourceForResponse(String logId, SclManager manager, String path, XoObject resource,
            URI requestingEntity, FilterCriteria filterCriteria, Set supported) throws UnsupportedEncodingException,
            XoException, StorageException {
        Condition condition = manager.getConditionBuilder().createStringCondition(Constants.ATTR_TYPE,
                ConditionBuilder.OPERATOR_EQUAL, Constants.TYPE_M2M_POC);
        SearchResult searchResult = manager.getStorageContext().search(path, StorageRequestExecutor.SCOPE_EXACT, condition);
        Map children = searchResult.getResults();
        generateNamedReferenceCollection(logId, manager, M2MPoc.getInstance(), requestingEntity, path, resource, children,
                M2MConstants.TAG_M2M_M2M_POC_COLLECTION);
    }

    public void doCreateIndication(SclManager manager, String path, XoObject resource, Indication indication)
            throws ParseException, IOException, StorageException, XoException, M2MException {
        if (Profiler.getInstance().isTraceEnabled()) {
            Profiler.getInstance().trace(
                    indication.getTransactionId() + ": Check " + M2MConstants.FLAG_CREATE + " right on " + path);
        }
        checkRights(indication.getTransactionId(), manager, path, resource, indication.getRequestingEntity(),
                M2MConstants.FLAG_CREATE);
        if (Profiler.getInstance().isTraceEnabled()) {
            Profiler.getInstance().trace(
                    indication.getTransactionId() + ": " + M2MConstants.FLAG_CREATE + " right granted on " + path);
        }

        XoObject representation = null;
        XoObject childResource = null;
        try {
            boolean sendRepresentation = false;
            representation = indication.getRepresentation(null);
            checkRepresentationNotNull(representation);

            // Check id
            String id = representation.getStringAttribute(M2MConstants.ATTR_M2M_ID);
            if (id == null) {
                sendRepresentation = true;
                id = UUID.randomUUID(16);
            }
            String childPath = path + M2MConstants.URI_SEP + id;

            SclLogger.logRequestIndication(Constants.PT_M2M_POC_CREATE_REQUEST, Constants.PT_M2M_POC_CREATE_RESPONSE,
                    indication, null, Constants.ID_LOG_REPRESENTATION);

            if (id.length() == 0) {
                throw new M2MException("id cannot be empty", StatusCode.STATUS_BAD_REQUEST);
            } else if (reservedKeywords.contains(id)) {
                throw new M2MException(id + " is a reserved keywork in m2mPocs resource", StatusCode.STATUS_BAD_REQUEST);
            } else if (id.indexOf('/') != -1) {
                throw new M2MException("id cannot contains a '/' character: " + id, StatusCode.STATUS_BAD_REQUEST);
            }

            byte[] testChildPath = manager.getStorageContext().retrieve(childPath);
            if (testChildPath != null) {
                sendUnsuccessResponse(manager, indication, StatusCode.STATUS_CONFLICT,
                        "A m2mPoc resource already exists with the id " + id);
            } else {
                M2MPoc childController = M2MPoc.getInstance();
                Date now = new Date();
                String creationTime = FormatUtils.formatDateTime(now, manager.getTimeZone());
                SclTransaction transaction = new SclTransaction(manager.getStorageContext());
                // TODO special case (must update onlineStatus of parent <scl> cf Stage3)
                childResource = manager.getXoService().newXmlXoObject(M2MConstants.TAG_M2M_M2M_POC);
                sendRepresentation = childController.createResource(manager, childPath, childResource, indication, path, id,
                        now, creationTime, representation, transaction) || sendRepresentation;

                resource.setStringAttribute(M2MConstants.TAG_M2M_LAST_MODIFIED_TIME, creationTime);

                Collection searchAttributes = new ArrayList();
                searchAttributes.add(new Pair(M2MConstants.ATTR_CREATION_TIME, FormatUtils.parseDateTime(resource
                        .getStringAttribute(M2MConstants.TAG_M2M_CREATION_TIME))));
                searchAttributes.add(new Pair(M2MConstants.ATTR_LAST_MODIFIED_TIME, now));
                transaction.updateResource(path, resource, searchAttributes);
                transaction.execute();

                childController.sendCreatedSuccessResponse(manager, childPath, childResource, indication, sendRepresentation);
            }
        } finally {
            if (representation != null) {
                representation.free(true);
            }
            if (childResource != null) {
                childResource.free(true);
            }
        }
    }

    public int appendDiscoveryURIs(String logId, SclManager manager, String path, XoObject resource, URI requestingEntity,
            URI targetID, String appPath, String[] searchStrings, List discoveryURIs, int remainingURIs) throws IOException,
            StorageException, XoException {
        int urisCount = remainingURIs;
        try {
            checkRights(logId, manager, path, resource, requestingEntity, M2MConstants.FLAG_DISCOVER);
            if (urisCount > 0) {
                discoveryURIs.add(appPath + URIUtils.encodePath(path));
            }
            --urisCount;
        } catch (M2MException e) {
            // Right is not granted
        }
        return urisCount;
    }
}
