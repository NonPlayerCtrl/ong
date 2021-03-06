
#include <stdio.h>      /************************************/
#include <stdlib.h>     /*     run in main thread           */
#include <string.h>     /************************************/
#include <stdarg.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <poll.h>
#include <string.h>


#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>


#include "rtlbase.h"
#include "rtllist.h"
#include "rtlimsg.h"

#include "admin_cli.h"
#include "song_over_http_api.h"
#include "cltSongImpl.h"

#define FD_MAX    CLI_MAX_FD
#define TCP_FRAME_SZ  1024


extern void *MainTbPoll;

t_cli *TbCli[FD_MAX];

static  int FdListen = -1;
static  int TcpKeepAlive  = 1;

static void AdmHelp(t_cli *cl)
{
  AdmPrint(cl, "help, ?     : display this help message\n");
  AdmPrint(cl, "exit, quit  : end the current CLI session\n");
  AdmPrint(cl, "core        : do generate a core file of the current process\n");
  AdmPrint(cl, "\n");
  AdmPrint(cl, "sendCreate <targetID> <reqEnt> <file> : do send a CREATE request to <uri>\n");
  AdmPrint(cl, "                                        request content is read from <file>\n");
  AdmPrint(cl, "sendRetrieve <targetID> <reqEnt>      : do send a RETRIEVE request to <uri>\n");
  AdmPrint(cl, "sendUpdate <targetID> <reqEnt> <file> : do send a UPDATE request to <uri>\n");
  AdmPrint(cl, "                                        request content is read from <file>\n");
  AdmPrint(cl, "sendDelete <targetID> <reqEnt>        : do send a DELETE request to <uri>\n");
  AdmPrint(cl, "sendCreate, sendRetrieve, sendUpdate and sendDelete may also have a last\n"
               "parameter <proxy>. If present, it specifies the NSCL host and port that\n"
               "will proxy the request toward the targetted GSCL.");
}

// "sendCreate <targetID> <reqEnt> <file> [<proxy>]"
void doSendCreate(t_cli *cl, char *buf)
{
  FILE *contentFile = NULL;
  char uri[255];
  char reqEntity[255];
  char file[255];
  char proxy[255];
  char *transId = NULL;
  char *proxyPtr = NULL;
  unsigned char content[4096];
  size_t readLen = 0;

  memset(uri, 0, sizeof(uri));
  memset(reqEntity, 0, sizeof(reqEntity));
  memset(file, 0, sizeof(file));
  memset(proxy, 0, sizeof(proxy));
  memset(content, 0, sizeof(content));

  buf = AdmAccept(buf);
  buf = AdmAdvance(buf);
  buf = AdmAcceptKeep(buf, uri);
  if (! *uri)
  {
    AdmPrint(cl, "<targetID> is missing\n");
    return;  
  }
  buf = AdmAdvance(buf);
  buf = AdmAcceptKeep(buf, reqEntity); 
  if (! *reqEntity)
  {
    AdmPrint(cl, "<reqEntity> is missing\n");
    return;  
  }
  buf = AdmAdvance(buf);
  buf = AdmAcceptKeep(buf, file); 
  if (! *file)
  {
    AdmPrint(cl, "<file> is missing\n");
    return;  
  }

  buf = AdmAdvance(buf);
  buf = AdmAcceptKeep(buf, proxy);
  if (*proxy)
  {
    proxyPtr = proxy;
  } 

  contentFile = fopen(file, "r");
  if (! contentFile)
  {
    AdmPrint(cl, "*** Cannot open file %s: %s (errno:%d)\n\n", file, strerror(errno), errno);
    return;
  }
  readLen = fread(content, 1, sizeof(content), contentFile); 
  fclose(contentFile);

  if (SOH_RC_OK == sohCreateRequest(proxyPtr, reqEntity, uri, content, readLen, 
        "application/xml", NULL, &transId, cl, testerCreateResponseCb))
  {
    AdmPrint(cl, "[-->] [transId:%s] [proxy:%s] CREATE sent\n", transId, proxyPtr);
  }
  else
  { 
    AdmPrint(cl, "[-X ] [transId:%s] failed to send CREATE\n", transId);
  }
  
}

// "sendRetrieve <targetID> <reqEnt>"
void doSendRetrieve(t_cli *cl, char *buf)
{
  char uri[255];
  char reqEntity[255];
  char proxy[255];
  char *proxyPtr = NULL;
  char *transId = NULL;

  memset(uri, 0, sizeof(uri));
  memset(reqEntity, 0, sizeof(reqEntity));
  memset(proxy, 0, sizeof(proxy));

  buf = AdmAccept(buf);
  buf = AdmAdvance(buf);
  buf = AdmAcceptKeep(buf, uri);
  if (! *uri)
  {
    AdmPrint(cl, "<targetID> is missing\n");
    return;  
  }
  buf = AdmAdvance(buf);
  buf = AdmAcceptKeep(buf, reqEntity); 
  if (! *reqEntity)
  {
    AdmPrint(cl, "<reqEntity> is missing\n");
    return;  
  }

  buf = AdmAdvance(buf);
  buf = AdmAcceptKeep(buf, proxy);
  if (*proxy)
  {
    proxyPtr = proxy;
  } 


  if (SOH_RC_OK == sohRetrieveRequest(proxyPtr, reqEntity, uri,
        NULL, &transId, cl, testerRetrieveResponseCb))
  {
    AdmPrint(cl, "[-->] [transId:%s] [proxy:%s] RETRIEVE sent\n", transId, proxyPtr);
  }
  else
  { 
    AdmPrint(cl, "[-X ] [transId:%s] failed to send RETRIEVE\n", transId);
  }
  
}

// "sendUpdate <targetID> <reqEnt> <file>"
void doSendUpdate(t_cli *cl, char *buf)
{
  FILE *contentFile = NULL;
  char uri[255];
  char reqEntity[255];
  char file[255];
  char proxy[255];
  char *proxyPtr = NULL;
  char *transId = NULL;
  unsigned char content[4096];
  size_t readLen = 0;

  memset(uri, 0, sizeof(uri));
  memset(reqEntity, 0, sizeof(reqEntity));
  memset(file, 0, sizeof(file));
  memset(content, 0, sizeof(content));
  memset(proxy, 0, sizeof(proxy));

  buf = AdmAccept(buf);
  buf = AdmAdvance(buf);
  buf = AdmAcceptKeep(buf, uri);
  if (! *uri)
  {
    AdmPrint(cl, "<targetID> is missing\n");
    return;  
  }
  buf = AdmAdvance(buf);
  buf = AdmAcceptKeep(buf, reqEntity); 
  if (! *reqEntity)
  {
    AdmPrint(cl, "<reqEntity> is missing\n");
    return;  
  }
  buf = AdmAdvance(buf);
  buf = AdmAcceptKeep(buf, file); 
  if (! *file)
  {
    AdmPrint(cl, "<file> is missing\n");
    return;  
  }

  buf = AdmAdvance(buf);
  buf = AdmAcceptKeep(buf, proxy);
  if (*proxy)
  {
    proxyPtr = proxy;
  } 

  contentFile = fopen(file, "r");
  if (! contentFile)
  {
    AdmPrint(cl, "*** Cannot open file %s: %s (errno:%d)\n\n", file, strerror(errno), errno);
    return;
  }
  readLen = fread(content, 1, sizeof(content), contentFile); 
  fclose(contentFile);

  if (SOH_RC_OK == sohUpdateRequest(proxyPtr, reqEntity, uri, content, readLen, 
        "application/xml", NULL, &transId, cl, testerUpdateResponseCb))
  {
    AdmPrint(cl, "[-->] [transId:%s] [proxy:%s] UPDATE sent\n", transId, proxyPtr);
  }
  else
  { 
    AdmPrint(cl, "[-X ] [transId:%s] failed to send UPDATE\n", transId);
  }
  
}

// "sendDelete <targetID> <reqEnt>"
void doSendDelete(t_cli *cl, char *buf)
{
  char uri[255];
  char reqEntity[255];
  char proxy[255];
  char *proxyPtr = NULL;
  char *transId = NULL;

  memset(uri, 0, sizeof(uri));
  memset(reqEntity, 0, sizeof(reqEntity));
  memset(proxy, 0, sizeof(proxy));

  buf = AdmAccept(buf);
  buf = AdmAdvance(buf);
  buf = AdmAcceptKeep(buf, uri);
  if (! *uri)
  {
    AdmPrint(cl, "<targetID> is missing\n");
    return;  
  }
  buf = AdmAdvance(buf);
  buf = AdmAcceptKeep(buf, reqEntity); 
  if (! *reqEntity)
  {
    AdmPrint(cl, "<reqEntity> is missing\n");
    return;  
  }

  buf = AdmAdvance(buf);
  buf = AdmAcceptKeep(buf, proxy);
  if (*proxy)
  {
    proxyPtr = proxy;
  } 

  if (SOH_RC_OK == sohDeleteRequest(proxyPtr, reqEntity, uri,
        NULL, &transId, cl, testerDeleteResponseCb))
  {
    AdmPrint(cl, "[-->] [transId:%s] [proxy:%s] DELETE sent\n", transId, proxyPtr);
  }
  else
  { 
    AdmPrint(cl, "[-X ] [transId:%s] failed to send DELETE\n", transId);
  }
  
}


/**********************************************************/
/* SUPPORTED COMMANDS */
static  int AdmCommand(t_cli *cl, char *buf)
{

  if  (!strlen(buf))
  {
    return  CLI_PROMPT;
  }

  if ((0 == strcmp(buf, "help")) || (0 == strcmp(buf, "?")))
  {
    AdmHelp(cl);
    return CLI_PROMPT;
  }
  if ((0 == strcmp(buf, "quit")) || (0 == strcmp(buf, "exit")))
  {
    return CLI_DISC;
  }
  if (0 == strcmp(buf, "core"))
  {
    rtl_docore();
    return CLI_PROMPT;
  }

  if (0 == strncmp(buf, "sendCreate ", strlen("sendCreate ")))
  {
    doSendCreate(cl, buf);
    return CLI_PROMPT;
  }
  if (0 == strncmp(buf, "sendRetrieve ", strlen("sendRetrieve ")))
  {
    doSendRetrieve(cl, buf);
    return CLI_PROMPT;
  }
  if (0 == strncmp(buf, "sendUpdate ", strlen("sendUpdate ")))
  {
    doSendUpdate(cl, buf);
    return CLI_PROMPT;
  }
  if (0 == strncmp(buf, "sendDelete ", strlen("sendDelete ")))
  {
    doSendDelete(cl, buf);
    return CLI_PROMPT;
  }

  return CLI_NFOUND;
}



static char *GetTelnetKey()
{
  return "";
}

static char *GetTelnetPort()
{
  return "2002";
}

static  void  AdmKeepAlive(int fd)
{
  if  ( TcpKeepAlive > 0 )
  {
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
        (char *)&TcpKeepAlive, sizeof(TcpKeepAlive));
#if 0
    if  ( TcpKeepIdle > 0 )
    {
      setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,
          (char *)&TcpKeepIdle, sizeof(TcpKeepIdle));
    }
    if  ( TcpKeepIntvl > 0 )
    {
      setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL,
          (char *)&TcpKeepIntvl, sizeof(TcpKeepIntvl));
    }
    if  ( TcpKeepCnt > 0 )
    {
      setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,
          (char *)&TcpKeepCnt, sizeof(TcpKeepCnt));
    }
#endif
  }
}

static  void  AdmAddBuf(t_cli *cl, char *buf, int len)
{
  t_lout  *out;

  RTL_TRDBG(9, "Adm add buffer lg=%d\n", len);
  if  (!buf || len <= 0)
  {
    return;
  }

  // buf deja decouper buf en morceaux plus petits que TCP_FRAME_SZ
  out = (t_lout *)malloc(sizeof(t_lout));
  if  ( !out )
  {
    return;
  }

  out->o_len  = len;
  out->o_buf  = (char *)malloc(len);
  if  ( !out->o_buf )
  {
    return;
  }
  memcpy  (out->o_buf, buf, len);

  list_add_tail( &(out->list), &(cl->cl_lout.list) );
}


int AdmTcpMsgCount(t_cli *cl)
{
  int nb  = 0;
  t_lout *msg;

  list_for_each_entry(msg, &cl->cl_lout.list, list)
  {
    nb++;
  }
  return  nb;
}

static  void  AdmAsyncWrite(t_cli *cl)
{
  int fd;
  t_lout  *lst;
  t_lout  *msg;
  int msgsz;
  int nb;
  int sz;
  char  out[TCP_FRAME_SZ];

  fd  = cl->cl_fd;
  lst = &cl->cl_lout;

  if  ( list_empty(&lst->list) )
  {
    return;
  }

  nb  = 0;
  sz  = 0;
  while ( !list_empty(&lst->list) )
  {
    msg = list_entry(lst->list.next, t_lout, list);
    msgsz = msg->o_len;
    if  ( sz + msgsz > sizeof(out) )
    {
      RTL_TRDBG(2, "defragmentation sz=%d no more than %d\n",
          sz + msgsz, sizeof(out));
      break;
    }
    list_del(&msg->list);
    memcpy  (out + sz, msg->o_buf, msgsz);
    sz  = sz + msgsz;
    nb++;
    free(msg->o_buf);
    free(msg);
  }

  if ( nb <= 0 )
  {
    RTL_TRDBG(0, "bad defragmentation nb=%d sz=%d\n", nb, sz);
    return;
  }

  if ( nb > 0 && sz > 0 )
  {
    rtl_timemono(&cl->cl_lasttcpwrite);
    RTL_TRDBG(9, "write adm/tcp nb=%d sz=%d\n", nb, sz);
    if (write(fd, out, sz) != sz)
    {
      RTL_TRDBG(0, "AdmAsyncWrite() write error\n");
    }
  }
}


char  *AdmAdvance(char *pt)
{
  while ( *pt && ( *pt == ' ' || *pt == '\t' ) )
  {
    pt++;
  }
  return  pt;
}

char  *AdmAccept(char *pt)
{
  while ( *pt && ( *pt != ' ' && *pt != '\t' ) )
  {
    pt++;
  }
  return  pt;
}

char  *AdmAcceptKeep(char *pt, char *dst)
{
  *dst  = '\0';
  while ( *pt && ( *pt != ' ' && *pt != '\t' ) )
  {
    *dst++  = *pt++;
  }
  *dst  = '\0';
  return  pt;
}


void  AdmPrintSync(int fd, char *fmt, ...)
{
  va_list listArg;
  char  tmp[5 * 1024] ;
  int lg ;

  va_start(listArg, fmt);
  vsprintf(tmp, fmt, listArg);
  va_end(listArg);

  lg  = strlen(tmp);
  if  ( tmp[lg - 1] == '\n' )
  {
    tmp[lg - 1] = '\r';
    tmp[lg]   = '\n';
    lg++;
  }

  if (write(fd, tmp, lg) != lg)
  {
    RTL_TRDBG(0, "AdmPrintSync() write error\n");
  }
}

void  AdmPrint(t_cli *cl, char *fmt, ...)
{
  va_list listArg;
  char  tmp[5 * 1024] ;
  int lg ;
  int out;
  int dep;

  if  ( (lg = strlen(fmt)) < 512 )
  {
    va_start(listArg, fmt);
    vsprintf(tmp, fmt, listArg);
    va_end(listArg);

    lg  = strlen(tmp);
    if  ( tmp[lg - 1] == '\n' )
    {
      tmp[lg - 1] = '\r';
      tmp[lg]   = '\n';
      lg++;
    }
  }
  else
  {
    // if fmt is more than 512 we assume there is no a format
    if  ( lg > sizeof(tmp) )
    {
      lg  = sizeof(tmp);
    }
    memcpy  (tmp, fmt, lg);
  }


  dep = 0;
  do
  {
    if  ( lg > TCP_FRAME_SZ / 2 )
    {
      out = TCP_FRAME_SZ / 2;
    }
    else
    {
      out = lg;
    }
    AdmAddBuf(cl, tmp + dep, out);
    dep = dep + out;
    lg  = lg - out;
  }
  while ( lg > 0 );

}


void  AdmPrintTable(t_cli *cl, int nb, int tab[], char *fmt, char pstart, char pend, char psep)
{
  int n;

  for (n = 0 ; n < nb ; n++)
  {
    if  (n == 0)
    {
      AdmPrint(cl, "%c", pstart);
    }
    AdmPrint(cl, fmt, tab[n]);
    if  (n == nb - 1)
    {
      AdmPrint(cl, "%c", pend);
    }
    else
    {
      AdmPrint(cl, "%c", psep);
    }
  }

}



void  AdmPrintDefTable(t_cli *cl, int nb, int tab[], char *fmt)
{
  return  AdmPrintTable(cl, nb, tab, fmt, '[', ']', ' ');
}

void  AdmPrintTableUS(t_cli *cl, int nb, unsigned short tab[], char *fmt, char pstart, char pend, char psep)
{
  int n;

  for (n = 0 ; n < nb ; n++)
  {
    if  (n == 0)
    {
      AdmPrint(cl, "%c", pstart);
    }
    AdmPrint(cl, fmt, tab[n]);
    if  (n == nb - 1)
    {
      AdmPrint(cl, "%c", pend);
    }
    else
    {
      AdmPrint(cl, "%c", psep);
    }
  }

}

void  AdmPrintDefTableUS(t_cli *cl, int nb, unsigned short tab[], char *fmt)
{
  return  AdmPrintTableUS(cl, nb, tab, fmt, '[', ']', ' ');
}


static  t_cli *CliNew(int fd)
{
  t_cli *cl;

  cl  = (t_cli *)malloc(sizeof(t_cli));
  if  (!cl)
  {
    RTL_TRDBG(0, "malloc failure\n");
    return  NULL;
  }
  memset  (cl, 0, sizeof(t_cli));

  cl->cl_fd = fd;
  rtl_timemono(&cl->cl_lasttcpread);
  INIT_LIST_HEAD(&cl->cl_lout.list);

  RTL_TRDBG(1, "new cli %x %d\n", cl, cl->cl_fd);

  return  cl;
}


static  void  CliFree(t_cli *cl)
{
  int nb;

  if  (!cl)
  {
    return;
  }
  RTL_TRDBG(1, "free cli %x fd=%d\n", cl, cl->cl_fd);

  if (cl->cl_fd > 0 && cl->cl_fd < FD_MAX)
  {
    TbCli[cl->cl_fd] = NULL;
    close(cl->cl_fd);
    rtl_pollRmv(MainTbPoll, cl->cl_fd) ;
  }

  // clear & free outgoing message list
  nb  = 0;
  while ( !list_empty(&cl->cl_lout.list) )
  {
    t_lout *msg;

    msg = list_entry(cl->cl_lout.list.next, t_lout, list);
    list_del(&msg->list);
    free(msg->o_buf);
    free(msg);
    nb++;
  }
  free(cl);

  RTL_TRDBG(1, "adm connection is now cleared (wmsg=%d)\n", nb);
}

static  int AdmPassword(t_cli *cl, char *buf)
{
  if  ( strcmp(GetTelnetKey(), buf) )
  {
    cl->cl_nbkey++;
    if  ( cl->cl_nbkey >= 3 )
    {
      return  CLI_DISC;
    }
    return  CLI_KEY;
  }
  cl->cl_okkey  = 1;
  cl->cl_nbkey  = 0;
  return  CLI_PROMPT;
}

static  void  AdmLoopCmd(t_cli *cl, char *buf, int lg)
{
  char  *ptn;
  char  *ptr;
  char  *pt;
  int ret;
  int stop;


  strcat  (cl->cl_cmd, buf);
  cl->cl_pos  += lg;

  ptr = strrchr(cl->cl_cmd, '\r');
  ptn = strrchr(cl->cl_cmd, '\n');
  if  ( !ptr && !ptn )
  {
    return;
  }

  if  ( ptr )
  {
    *ptr  = '\0';
  }
  if  ( ptn )
  {
    *ptn  = '\0';
  }

  RTL_TRDBG(5, "adm get command '%s'\n", cl->cl_cmd);
  stop  = 0;
  pt  = cl->cl_cmd;
  while (!stop)
  {
    while ( *pt == '\n' || *pt == '\r' || *pt == '\t' )
    {
      pt++;
    }
    lg  = 0;
    while ( *pt )
    {
      if  ( *pt == '\n' || *pt == '\r' || *pt == '\t' )
      {
        pt++;
        break;
      }
      buf[lg] = *pt;
      pt++;
      lg++;
    }
    buf[lg] = '\0';
    if (strlen(buf))
    {
      RTL_TRDBG(1, "adm treat '%s'\n", buf);
    }

    if  ( !cl->cl_okkey && strlen(GetTelnetKey()) )
    {
      ret = AdmPassword(cl, buf);
    }
    else
    {
      ret = AdmCommand(cl, buf);
    }
    switch  (ret)
    {
      case  CLI_NFOUND  :
        AdmPrint(cl, "command not found\r\n");
        AdmPrint(cl, ">>");
        break;
      case  CLI_PROMPT  :
        AdmPrint(cl, ">>");
        break;
      case  CLI_DISC  :
        CliFree(cl);
        stop  = 1;
        cl  = NULL;
        break;
      case  CLI_KEY :
        AdmPrint(cl, "key:");
        break;
      case  CLI_EMPTY :
        AdmPrint(cl, ">>");
        break;
      case  CLI_MORE :
        AdmPrint(cl, "++");
        break;
    }
    if  ( !*pt )
    {
      break;
    }
  }
  if  (cl)
  {
    cl->cl_pos  = 0;
    memset  (cl->cl_cmd, 0, sizeof(cl->cl_cmd));
  }
}

int CB_AdmTcpRequest(void *tb, int fd, void *r1, void *r2, int revents)
{
  t_cli *cl;
  int empty;

  if  (fd >= FD_MAX || (cl = TbCli[fd]) == NULL)
  {
    RTL_TRDBG(0, "adm no cli for fd=%d\n", fd);
    rtl_pollRmv(MainTbPoll, fd);
    return  -1;
  }

  empty = list_empty(&cl->cl_lout.list);
  RTL_TRDBG(9, "callback request adm/tcp fd=%d empty=%d e=%d\n",
      fd, empty, revents);

  // if we have something to write ask for POLLOUT event and POLLIN
  // or just POLLOUT if we want to slow down remote writer
  if  ( !empty )
  {
    return  POLLIN | POLLOUT;
  }

  return  POLLIN;
}

int CB_AdmTcpEvent(void *tb, int fd, void *r1, void *r2, int revents)
{
  t_cli *cl;
  int lg;
  char  buf[500];

  RTL_TRDBG(9, "callback event adm/tcp fd=%d e=%d r1=%lx r2=%lx\n",
      fd, revents, r1, r2);

  if  (fd >= FD_MAX || (cl = TbCli[fd]) == NULL)
  {
    RTL_TRDBG(0, "adm no cli for fd=%d\n", fd);
    rtl_pollRmv(MainTbPoll, fd);
    return  -1;
  }

  if ((revents & POLLOUT) == POLLOUT)
  {
    RTL_TRDBG(9, "event write adm/tcp fd=%d\n", fd);
    AdmAsyncWrite(cl);
  }

  if ((revents & POLLIN) != POLLIN)
  {
    return  0;
  }
  lg  = read(fd, buf, sizeof(buf));
  if  ( lg <= 0 )
  {
    RTL_TRDBG(1, "Adm connection ended by remote\n");
    CliFree(cl);
    return  0;
  }
  buf[lg] = '\0';
  RTL_TRDBG(5, "event read adm/tcp fd=%d lg=%d\n", fd, lg);
  rtl_timemono(&cl->cl_lasttcpread);



  if  ( cl->cl_pos + lg >= sizeof(cl->cl_cmd) )
  {
    RTL_TRDBG(1, "command too long => reset\n");
    CliFree(cl);
    return  0;
  }
  AdmLoopCmd(cl, buf, lg);


  return  0;
}

static  void  SetCB(int fdnew)
{
  rtl_pollAdd(MainTbPoll, fdnew, CB_AdmTcpEvent, CB_AdmTcpRequest, NULL, NULL);
}

int CB_AdmTcpListen(void *tb, int fd, void *r1, void *r2, int revents)
{
  t_cli *cl;
  int fdnew;
  struct sockaddr_in6 from;
  socklen_t lenaddr;

  RTL_TRDBG(1, "callback event adm/tcp fd=%d FD=%d e=%d r1=%lx r2=%lx\n",
      fd, FdListen, revents, r1, r2);

  if (fd != FdListen)
  {
    RTL_TRDBG(0, "fd=%d != myfd=%d\n", fd, FdListen);
    rtl_pollRmv(MainTbPoll, fd);
    return  -1;
  }

  if ((revents & POLLIN) != POLLIN)
  {
    return  0;
  }

  lenaddr = sizeof(from) ;
  fdnew = accept(FdListen, (struct sockaddr *)&from, &lenaddr);
  if  ( fdnew < 0 )
  {
    return  0;
  }

  if  (fdnew > FD_MAX)
  {
    AdmPrintSync(fdnew, "Adm does not support fd > %d\r\n", FD_MAX);
    close(fdnew);
    RTL_TRDBG(0, "Adm does not support fd > %d\r\n", FD_MAX);
    return  0;
  }
  if  (TbCli[fdnew] != NULL)
  {
    close(fdnew);
    RTL_TRDBG(0, "Amd fd=%d not free !!!\r\n", fdnew);
    return  0;
  }

  cl  = CliNew(fdnew);
  if  (!cl)
  {
    close(fdnew);
    return  0;
  }
  TbCli[fdnew]  = cl;

  AdmKeepAlive(fdnew);

  SetCB(fdnew);

  AdmPrint(cl, "Welcome to libSongOverHttp tester CLI\n");
  if  (strlen(GetTelnetKey()))
  {
    AdmPrint(cl, "key:");
  }
  else
  {
    AdmPrint(cl, ">>");
  }

  return  0;
}

static  void  AdmTcpBind()
{
  int flag;

  struct addrinfo hints;
  struct addrinfo *result, *rp;


  char  *strport  = GetTelnetPort();

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
  hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */

  if (getaddrinfo(NULL, strport, &hints, &result))
  {
    RTL_TRDBG(0, "adm/tcp cannot getaddrinfo port=%s\n",
        strport);
    return;
  }
  for (rp = result; rp != NULL; rp = rp->ai_next)
  {
    FdListen = socket(rp->ai_family, rp->ai_socktype,
        rp->ai_protocol);
    if  (FdListen < 0)
    {
      continue;
    }
    flag  = 1;
    if  ( setsockopt(FdListen, SOL_SOCKET, SO_REUSEADDR,
        (char *)&flag, sizeof(flag)) < 0)
    {
      close (FdListen) ;
      continue;
    }
    if  (bind(FdListen, rp->ai_addr, rp->ai_addrlen) == 0)
    {
      RTL_TRDBG(0, "ok bind adm/tcp\n");
      break;
    }
  }
  if (rp == NULL)
  {
    FdListen  = -1;
    RTL_TRDBG(0, "cannot bind adm/tcp '%s'+%s\n", "*", strport);
    return;
  }

  if (listen(FdListen, 2) < 0)
  {
    close(FdListen);
    FdListen  = -1;
    RTL_TRDBG(0, "cannot listen adm/tcp '%s'+%s\n", "*", strport);
    return;
  }

  rtl_pollAdd(MainTbPoll, FdListen, CB_AdmTcpListen, NULL, NULL, NULL);
}

void  AdmTcpInit()
{
  AdmTcpBind();
}

void  AdmClockSc(time_t now)
{
  t_cli *wh;
  int i = 0;
  time_t  diff;
  time_t  delta;
  time_t  timer = 600;

  delta = timer - (timer / 10);
  for (i = 0 ; i < FD_MAX ; i++)
  {
    wh  = TbCli[i];
    if  (!wh)
    {
      continue;
    }

    // remote client does not read its messages
    diff  = ABS(now - wh->cl_lasttcpwrite);
    if  (diff >= 60 && AdmTcpMsgCount(wh) > 0)
    {
      RTL_TRDBG(1, "waiting msg timer for [%03d]\n", i);
      CliFree(wh);
      continue;
    }

    // remote client does not send messages
    diff  = ABS(now - wh->cl_lasttcpread);
    if  (diff >= timer)
    {
      RTL_TRDBG(1, "Inactivity timer for [%03d]\n", i);
      CliFree(wh);
      continue;
    }
    if  ( diff >= delta )
    {
      AdmPrint(wh,
          "you'll be disconnected in %d sec!!!\n",
          timer - diff);
    }
  }
}









