/*
 * Copyright (C) Actility, SA. All Rights Reserved.
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <poll.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "_whatstr.h"

#include "rtlbase.h"
#include "rtlimsg.h"
#include "xoapipr.h"
#include "dIa.h"

#include "adaptor.h"
#include "wmbus.h"

#include "define.h"
#include "cmnstruct.h"

#include "cproto.h"
#include "extern.h"
#include "sensor.h"
#include "covconfig.h"
#include "extension_library.h"
#include "extension_manager.h"


int TraceLevel = 3; //2;
int TraceDebug = 3; //1;
int TraceProto = 3; //0;
int TraceSize = 200 * 1024; // for each file
int PseudoOng = 0;
int WithDia = 1;
int Operating = 1;  // 1 for ong deploy, 0 for dev
int resetNwkAtReboot = 0;
int running = 1;

int DiaTraceRequest = 0;

void *MainTbPoll;
void *MainIQ;
void *MdlCfg;
extensionManager_t *preprocessorMgmt = NULL;

unsigned int  DiaIpuOk;
unsigned int  DiaNetOk;
unsigned int  DiaIpuTmt;
unsigned int  DiaNetTmt;
unsigned int  DiaIpuErr;
unsigned int  DiaNetErr;
unsigned int  DiaRepDrp;

char *knownSensorPath = NULL;
char *rootactPath = NULL;

void wmbusDongleError(void);
int wmbusDongleStop(void);
int wmbusDongleStart(void);
void wmbusDongleClockSc(time_t now);

/*
 *  Use only by drvcommon when Operating == 0
 */
char *
GetEnvVarName(void)
{
  if (!Operating)
  {
    return "ROOTACT";
  }

  return NULL;
}

/*!
    *
    * @brief calculate the next serial number which can be used to identity any
    * object/structure/messsage. 0 is not a valid serial number. Mainly used for
    * t_sensor structure to identity sensors in conjonction with a sensor number.
    * @return [1..INT_MAX]
    *
    */
int DoSerial()
{
  static int Serial = INT_MAX;

  if (Serial == INT_MAX)
  {
    Serial = 1;
  }
  else
  {
    Serial++;
  }

  return Serial;
}

/*!
    *
    * @brief called by main loop every 100 ms.
    * @return void
    *
    */
void DoClockMs()
{

}

/*!
    *
    * @brief called by main loop every 1 second.
    * @return void
    *
    */
void DoClockSc()
{
  static unsigned int nbclock = 0;
  time_t now = 0;

  nbclock++;
  rtl_timemono(&now);

  AdmClockSc(now);
  DiaClockSc(now);
  wmbusDongleClockSc(now);
  WmbusSensorClockSc(now);
}

/*!
    *
    * @brief called by main loop to dispatch an internal message
    * @return void
    *
    */
void
DoInternalEvent(t_imsg *imsg)
{
  RTL_TRDBG(TRACE_API, "receive event cl=%d ty=%d\n", imsg->im_class, imsg->im_type);

  switch (imsg->im_class)
  {
    case IM_DRIVER:
      switch (imsg->im_type)
      {
          // Dongle is configured, Start Init of M2M
        case IM_DONGLE_READY:
//          DiaIpuStart();
          break;

        case IM_DONGLE_CONFIGURATION_TIMEOUT:
          RTL_TRDBG(TRACE_API, "Timeout on wMBus configuration\n");
          wmbusDongleError();
          wmbusDongleStart();
          break;
      }
      break;
  }
}

/*!
    *
    * @brief called by main loop to dispatch an internal timer
    * @return void
    *
    */
void
DoInternalTimer(t_imsg *imsg)
{
  RTL_TRDBG(TRACE_API, "receive timer cl=%d ty=%d vsize=%ld\n", imsg->im_class, imsg->im_type, rtl_vsize(getpid()));

  switch (imsg->im_class)
  {
    case IM_DEF :
      switch (imsg->im_type)
      {
        case IM_TIMER_GEN:
          rtl_imsgAdd(
            MainIQ,
            rtl_timerAlloc(IM_DEF, IM_TIMER_GEN, IM_TIMER_GEN_V, NULL, 0)
          );
          break;
      }
      break;

    case IM_SENSOR:
      WmbusSensorEvent(imsg, NULL);
      break;
  }
}

/*!
    *
    * @brief the loop of the main thread
    * - treat internal events
    * - call rtl_poll() to wait for external events
    * - call DoClockMs() and DoClockSc() if necessary
    * - treat internal timers
    * @return void
    *
    */
void
MainLoop()
{
  time_t lasttimems = 0;
  time_t lasttimesc = 0;
  time_t now = 0;
  // int ret;

  t_imsg *msg;

  while (running)
  {
    // internal events
    while ((msg = rtl_imsgGet(MainIQ, IMSG_MSG)) != NULL)
    {
      DoInternalEvent(msg);
      rtl_imsgFree(msg);
    }

    // external events
    /* ret = */ rtl_poll(MainTbPoll, MSTIC);

    // clocks
    now = rtl_tmmsmono();
    if (ABS(now - lasttimems) >= 100)
    {
      DoClockMs();
      lasttimems = now;
    }
    if (ABS(now - lasttimesc) >= 1000)
    {
      DoClockSc();
      lasttimesc = now;
    }

    // internal timer
    while ((msg = rtl_imsgGet(MainIQ, IMSG_TIMER)) != NULL)
    {
      DoInternalTimer(msg);
      rtl_imsgFree(msg);
    }
  }
}

void
Usage(char *prg, char *fmt)
{
  printf("usage: %s [-h|--help] [-a <USB-device>] [-t <level>] [-d <0|1>] [-D] [-O] [-X] [-i]\n",
      prg);
  printf("Where:\n");
  printf("\t-h|--help      : print this help message\n");
  printf("\t-a <USB-device>: set the USB TTY to listen to. (default: /dev/ttyUSB0)\n");
  printf("\t-t <level>     : set the trace level to <level>\n");
  printf("\t-d <0|1>       : disable/enable trace debugging\n");
  printf("\t-D             : disable DIA layer\n");
  printf("\t-O             : enable pseudo ONG running mode\n");
  printf("\t-X             : enable trace on DIA requests/responses\n");
  printf("\t-i             : reset area network at start-up (if not provided the \n");
  printf("\t                 HAN is resumed from previous run)\n");
  printf("\t--version      : print the version string of the wMBus driver\n");
  printf("\t                 and its dependencies\n");
}

/*!
    *
    * @brief analyze command line options.
    * @return int
    *
    */
int
DoArg(int initial, int argc, char *argv[])
{
  extern char *optarg;
  extern int optind;
  int c;
  char *pt;

  char *fmtgetopt = "hXDOSHit:d:p:a:";

  int i;

  for (i = 1 ; i < argc ; i++)
  {
    if (strcmp(argv[i], "--version") == 0)
    {
      pt = AwVersion();
      printf ("%s\n", rtl_version());
      printf ("%s\n", XoVersion());
      printf ("%s\n", dia_getVersion());
      printf ("%s\n", pt);
      if (strcmp(pt, wmbus_whatStr) != 0)
      {
        printf ("%s\n", wmbus_whatStr);
        printf ("compilation required\n");
      }
      exit(0);
    }
    if (strcmp(argv[i], "--help") == 0)
    {
      Usage(argv[0], fmtgetopt);
      exit(0);
    }
  }

  if (initial)
  {
    return argc;
  }

  while ((c = getopt (argc, argv, fmtgetopt)) != -1)
  {
    switch (c)
    {
      case 'a':
        RTL_TRDBG(TRACE_ERROR, "wMBus Dongle forced on TTY : %s\n", optarg);
        SetVar("w_device_tty", optarg);
        break;

      case 'h' :
        Usage(argv[0], fmtgetopt);
        exit(0);
        break;

      case 't' :
        TraceLevel = atoi(optarg);
        break;

      case 'd' :
        TraceDebug = atoi(optarg);
        break;

      case 'p' :
        TraceProto = atoi(optarg);
        break;

      case 'D' :
        WithDia = 0;
        break;

      case 'O' :
        PseudoOng = 1;
        break;

      case 'X' :
        DiaTraceRequest = 1;
        break;

      case  'i' :
        resetNwkAtReboot = 1;
        break;

      default:
        break;
    }
  }

  return argc;
}


/*!
    *
    * @brief command line options can also be set/unset with configuration files.
    * @return void
    *
    */
void SetOption(char *name, char *value)
{
  if (strcmp(name, "tracelevel") == 0)
  {
    TraceLevel = atoi(value);
    return;
  }

  if (strcmp(name, "tracedebug") == 0)
  {
    TraceDebug = atoi(value);
    return;
  }

  if (strcmp(name, "tracesize") == 0)
  {
    TraceSize = atoi(value);
    return;
  }

  if (strcmp(name, "withdia") == 0)
  {
    WithDia = atoi(value);
    return;
  }

  if (strcmp(name, "tracedia") == 0)
  {
    DiaTraceRequest = atoi(value);
    return;
  }

  if (strcmp(name, "pseudoong") == 0)
  {
    PseudoOng = atoi(value);
    return;
  }

  RTL_TRDBG(TRACE_ERROR, "ERROR parameter/option '%s' not found\n", name);
}

/*!
    *
    * @brief analyze configuration files for command line options and variables
    * @param hot cold or hot configuration phase
    * @param custom analyzing the custom or the default configuration file
    * @param cfg the file configuration path
    * @return void
    *
    */
void DoConfig(int hot, int custom, char *fcfg)
{
  void *o;
  void *var;
  int parseerr;
  int parseflags = XOML_PARSE_OBIX;

  int nb;
  int i;

  o = XoReadXmlEx(fcfg, NULL, parseflags, &parseerr);
  if (parseerr > 0)
  {
    RTL_TRDBG(TRACE_ERROR, "ERROR config file '%s' parsing error=%d \n",
        fcfg, parseerr);
    return;
  }
  if (parseerr < 0)
  {
    RTL_TRDBG(TRACE_ERROR, "ERROR config file '%s' loading error=%d \n",
        fcfg, parseerr);
    return;
  }

  if (!o)
  {
    RTL_TRDBG(TRACE_ERROR, "ERROR config file '%s' not found\n", fcfg);
    return;
  }

  nb = XoNmNbInAttr(o, "[name=parameters].[]", 0);
  for (i = 0 ; i < nb ; i++)
  {
    char *name;
    char *value;

    var = XoNmGetAttr(o, "[name=parameters].[%d]", 0, i);
    if (!var)
    {
      continue;
    }

    name = XoNmGetAttr(var, "name", 0);
    value = XoNmGetAttr(var, "val", 0);

    RTL_TRDBG(TRACE_ERROR, "hot=%d cust=%d parameter='%s' value='%s'\n",
        hot, custom, name, value);
    if (name && *name && value && *value)
    {
      SetOption(name, value);
    }
  }

  nb = XoNmNbInAttr(o, "[name=variables].[]", 0);
  for (i = 0 ; i < nb ; i++)
  {
    char *name;
    char *value;

    var = XoNmGetAttr(o, "[name=variables].[%d]", 0, i);
    if (!var)
    {
      continue;
    }

    name = XoNmGetAttr(var, "name", 0);
    value = XoNmGetAttr(var, "val", 0);

    RTL_TRDBG(TRACE_ERROR, "hot=%d cust=%d variable='%s' value='%s'\n",
        hot, custom, name, value);
    if (name && *name && value && *value)
    {
      SetVar(name, value);
    }
  }

  // custom configuration can not change m2m mapping
  nb  = XoNmNbInAttr(o, "[name=mapping].[]", 0);
  for (i = 0 ; custom == 0 && i < nb ; i++)
  {
    char  *name;
    char  *value;

    var = XoNmGetAttr(o, "[name=mapping].[%d]", 0, i);
    if  (!var)
    {
      continue;
    }

    name  = XoNmGetAttr(var, "name", 0);
    value = XoNmGetAttr(var, "val", 0);

    RTL_TRDBG(TRACE_ERROR, "hot=%d cust=%d mapping='%s' value='%s'\n",
        hot, custom, name, value);
    if  (name && *name && value && *value)
    {
      AddVar(name);
      SetVar(name, value);
    }
  }

  XoFree  (o, 1);
}


/**
 * @brief Delete all files of known nodes.
 * This operation is done when resetting the Network.
 * @param path the path to the known-nodes files.
 */
static void deleteKnownNodes(char *path)
{
  void *dir;
  char *name;
  char *point;
  char fullPath[512];

  dir = rtl_openDir(path);
  if (!dir)
  {
    return;
  }

  while ( (name = rtl_readDir(dir)) && *name )
  {
    if (*name == '.' && *(name + 1) == '\0')
    {
      continue;
    }
    if (*name == '.' && *(name + 1) == '.')
    {
      continue;
    }
    point = rtl_suffixname(name);
    if ( point && *point && strcmp(point + 1, "xml") == 0 )
    {
      sprintf(fullPath, "%s/%s", path, name);
      remove(fullPath);
    }
  }
}

/**
 * Stop the driver's main loop.
 */
void stopService(int sig)
{
  RTL_TRDBG(TRACE_ERROR, "Service stopped (sig:%d)\n", sig);
  printf("Service stopped (sig:%d)\n", sig);
  running = 0;
}


/*!
    *
    * @brief watteco sensors supervision
    * - verify $ROOTACT and $ environment variables
    * - first analyze of command line options (for --version)
    * - initialize traces module
    * - initialize poll and timers/messages modules
    * - initialize xo module and load M2M and adaptor specific definitions
    * - initialize variables module
    * - load default configuration
    * - load custom configuration
    * - second analyze of command line options to force some options
    * - initialize zcl/udp module
    * - initialize adm/tcp module
    * - initialize dia/udp module
    * - start http thread
    * - create sensor for knwon sensors list
    * - start DIA request for IPU and NETWORK
    * - allocate general timer
    * - enter main loop
    *
    * @return int
    *
    */
int main(int argc, char *argv[])
{
  int custom;
  char cfgroot[512];
  char cfgcust[512];
  char tmp[512];
  int initial;
  int hot;
  int i;

  signal(SIGHUP, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGTERM, stopService);

  rootactPath = getenv("ROOTACT");
  if ((!rootactPath || !*rootactPath))
  {
    printf("ROOTACT unset, abort\n");
    exit(1);
  }

  if (!Operating)
  {
    sprintf(cfgroot, "%s%s/config/spvconfig.xml", rootactPath, GetAdaptorName());
    cfgcust[0] = '\0';
  }
  else
  {
    sprintf(cfgroot, "%s/etc/%s/spvconfig.xml", rootactPath, GetAdaptorName());
    sprintf(cfgcust, "%s/usr/etc/%s/spvconfig.xml", rootactPath, GetAdaptorName());
  }
  DoArg(initial = 1, argc, argv);

  // Ensure wmbus knownsensors folder is created
  if (!Operating)
  {
    sprintf(tmp, "%s/knownsensors", getenv("AW_DATA_DIR"));
  }
  else
  {
    sprintf(tmp, "%s/usr/data/%s/knownsensors", rootactPath, GetAdaptorName());
  }
  rtl_mkdirp(tmp);
  knownSensorPath = malloc(strlen(tmp) + 1);
  sprintf(knownSensorPath, tmp);
  rtl_mkdirp(tmp);

  rtl_init();

  // Init rotate log
  rtl_tracemutex();
  rtl_tracelevel(TraceLevel);
  rtl_tracesizemax(TraceSize);
  if (!Operating)
  {
    sprintf(tmp, "%s%s/supervisor", rootactPath, GetAdaptorName());
  }
  else
  {
    sprintf(tmp, "%s/var/log/%s", rootactPath, GetAdaptorName());
  }
  rtl_mkdirp(tmp);
  strcat(tmp, "/");
  strcat(tmp, "TRACE.log");
  rtl_tracerotate(tmp);

  RTL_TRDBG(TRACE_ERROR, "start %s/main th=%lx pid=%d stack=%p\n", argv[0],
      (long)pthread_self(), getpid(), &custom);
  RTL_TRDBG(TRACE_ERROR, "%s\n", rtl_version());
  RTL_TRDBG(TRACE_ERROR, "%s\n", XoVersion());
  RTL_TRDBG(TRACE_ERROR, "%s\n", dia_getVersion());
  RTL_TRDBG(TRACE_ERROR, "%s\n", AwVersion());

  preprocessorMgmt = new_extensionManager_t();
  MainTbPoll = rtl_pollInit();
  MainIQ = rtl_imsgInitIq();

  XoInit(0);
  if (!Operating)
  {
    sprintf(tmp, "%s%s/xo/.", rootactPath, GetAdaptorName());
    XoLoadNameSpaceDir(tmp);
    XoLoadNameSpaceDir(".");
  }
  else
  {
    sprintf(tmp, "%s/etc/xo/.", rootactPath);
    XoLoadNameSpaceDir(tmp);
    sprintf(tmp, "%s/etc/%s/.", rootactPath, GetAdaptorName());
    XoLoadNameSpaceDir(tmp);
  }

  if (LoadM2MXoRef() < 0)
  {
    exit(1);
  }

  if (LoadDrvXoRef(GetAdaptorName()) < 0)
  {
    exit(1);
  }

  VarInit(); // before DoConfig()

  // get host name and set variable accordingly, before DoConfig()
  if (0 == gethostname(tmp, sizeof(tmp)))
  {
    SetVar("w_boxname", tmp);
  }
  else
  {
    RTL_TRDBG(TRACE_ERROR, "Unable to get the host name; "
        "driver will run with erroneous host name %s",
        GetVar("w_boxname"));
  }


  DoConfig(hot = 0, custom = 0, cfgroot);
  if (Operating)
  {
    DoConfig(hot = 0, custom = 1, cfgcust);
  }

  // convert boxname and domainname to lowercase
  sprintf(tmp, "%s", GetVar("w_boxname"));
  for (i = 0; tmp[i]; i++)
  {
    tmp[i] = tolower(tmp[i]);
  }
  SetVar("w_boxname", tmp);
  sprintf(tmp, "%s", GetVar("w_domainname"));
  for (i = 0; tmp[i]; i++)
  {
    tmp[i] = tolower(tmp[i]);
  }
  SetVar("w_domainname", tmp);

  DoArg(initial = 0, argc, argv);
  rtl_tracelevel(TraceLevel);
  rtl_tracesizemax(TraceSize);

  RTL_TRDBG(TRACE_ERROR, "TraceLevel=%d\n", TraceLevel);
  RTL_TRDBG(TRACE_ERROR, "TraceSize=%d\n", TraceSize);
  RTL_TRDBG(TRACE_ERROR, "TraceDebug=%d\n", TraceDebug);
  RTL_TRDBG(TRACE_ERROR, "TraceProto=%d\n", TraceProto);
  RTL_TRDBG(TRACE_ERROR, "TraceDia=%d\n", DiaTraceRequest);
  RTL_TRDBG(TRACE_ERROR, "WithDia=%d\n", WithDia);
  RTL_TRDBG(TRACE_ERROR, "PseudoOng=%d\n", PseudoOng);
  RTL_TRDBG(TRACE_ERROR, "ResetNwkAtReboot=%d\n", resetNwkAtReboot);

  // Init m2m modeling
  RTL_TRDBG(TRACE_ERROR, "Loading config modeling file\n");
  MdlCfg = MdlLoad(hot = 0);
  if (!MdlCfg)
  {
    exit(1);
  }

  // Init Cov config from modeling
  CovInit(MdlCfg);

  if (resetNwkAtReboot)
  {
    deleteKnownNodes(knownSensorPath);
  }

  // Init wMBus specific
  SetVar("w_netid", "dungleID");

  AdmTcpInit();
  AdmTcpSetCB(AdmCmd);

  if (WithDia)
  {
    DiaUdpInit();
  }

  // we start with known sensors
  SensorCreateKnown();

  rtl_imsgAdd(
    MainIQ,
    rtl_timerAlloc(IM_DEF, IM_TIMER_GEN, IM_TIMER_GEN_V, NULL, 0)
  );

  MainLoop();
  RTL_TRDBG(TRACE_ERROR, "end !!! %s/main th=%lx\n", argv[0], (long)pthread_self());

  wmbusDongleStop();

  preprocessorMgmt->free(preprocessorMgmt);

  exit(1);
}
