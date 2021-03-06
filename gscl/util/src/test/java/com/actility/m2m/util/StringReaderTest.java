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
 * id $Id: StringReaderTest.java 6057 2013-10-14 11:50:13Z mlouiset $
 * author $Author: mlouiset $
 * version $Revision: 6057 $
 * lastrevision $Date: 2013-10-14 13:50:13 +0200 (Mon, 14 Oct 2013) $
 * modifiedby $LastChangedBy: mlouiset $
 * lastmodified $LastChangedDate: 2013-10-14 13:50:13 +0200 (Mon, 14 Oct 2013) $
 */

package com.actility.m2m.util;

import java.text.ParseException;

import com.actility.m2m.util.StringReader;

import junit.framework.TestCase;
import junit.framework.TestSuite;

public class StringReaderTest extends TestCase {

    public void testSkipUntil() {
        try {
            String message = "test ; test=test";
            StringReader reader = new StringReader(message);
            assertEquals("test ", reader.readUntil(';'));
            reader.skipOffset(1);
            assertEquals(" test", reader.readUntil('='));
            reader.skipOffset(1);
            assertEquals("test", reader.readUntil('/'));
        } catch (ParseException e) {
            fail();
        }
    }

    /**
     * Launch the test.
     *
     * @param args the command line arguments
     *
     * @generatedBy CodePro at 10/5/11 11:27 PM
     */
    public static void main(String[] args) {
        if (args.length == 0) {
            // Run all of the tests
            junit.textui.TestRunner.run(StringReaderTest.class);
        } else {
            // Run only the named tests
            TestSuite suite = new TestSuite("Selected tests");
            for (int i = 0; i < args.length; i++) {
                TestCase test = new StringReaderTest();
                test.setName(args[i]);
                suite.addTest(test);
            }
            junit.textui.TestRunner.run(suite);
        }
    }
}
