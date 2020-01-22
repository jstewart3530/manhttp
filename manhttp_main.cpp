/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
  manhttp 

  An HTTP server that provides a web-based, hypertext-driven
  frontend for man(1), info(1), and apropos(1).  MANHTTP lets you
  browse and search your system's online documentation using a web
  browser.

~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~

  Library dependencies:

  libmicrohttpd      HTTP server library
                       https://www.gnu.org/software/libmicrohttpd/

  TRE                Regular expression engine
                       https://laurikari.net/tre/  
                       https://github.com/laurikari/tre/

  POPT               Library for parsing command-line arguments
                       https://github.com/devzero2000/POPT
                       https://linux.die.net/man/3/popt 

~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~

  Copyright 2019 Jonathan Stewart
  
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
  
      http://www.apache.org/licenses/LICENSE-2.0
  
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~

  https://github.com/jstewart3530/manhttp
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/


#include <stdlib.h>                    /*  C/C++ RTL headers.  */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <errno.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <popt.h>                      /*  Library headers.  */
#include <microhttpd.h>

#include "utility.h"                   /*  Application headers.  */
#include "html_formatting.h"
#include "man_page_api.h"
#include "manualpagetohtml.h"
#include "apropostohtml.h"
#include "infotohtml.h"
#include "installation.h"



/*  Dynamically-generated headers.
*/

#include "dynamic/stylesheet_text.h"
#include "dynamic/splash_html.h"
#include "dynamic/favicon.h"



#define MAX_THREADS     100



typedef struct sockaddr_in INETADDRESS;



/*  Text displayed along with the POPT help info.
*/

static const char HelpIntro [] 
        = "manhttp is a server that provides a browser-based frontend\n"
          "for man(1), info(1), and apropos(1).\n";



/*  Semi-global variables.
*/ 

static bool fReadyToQuit = false;
static int fUseSyslog = 0;
static char CachePolicy [64];
static char *pUriPrefix;
static const char *pStylesheet;
static const char *pFontDirectory = NULL;



/*  Function prototypes.
*/ 

static void ReportError (const char*, ...)
       __attribute__ ((format (printf, 1, 2)));
static int HandleRequest (void*, struct MHD_Connection*, const char*, const char*,
                          const char*, const char*, size_t*, void**);
static void HandleFontRequest (struct MHD_Connection*, const char*);
static const char* FontTypeFromFilename (const char*);
static void HandleManPageRequest (struct MHD_Connection*, const char*);
static void HandleInfoRequest (struct MHD_Connection*, const char*); 
static void HandleAproposRequest (struct MHD_Connection*, const char*); 
static void GenerateSplashPage (struct MHD_Connection*, const char*);
static void HandleInternalError (struct MHD_Connection*, const char*, const PROCESSERRORINFO*);
static void GenerateErrorPage (struct MHD_Connection*, const char*, 
                               int, const char*, ...)
       __attribute__ ((format (printf, 4, 5)));;



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                                     main
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

int main
   (int     argc,
    char  **argv)

{
  int t, fdSocket;
  uint32_t IPAddress = 0;
  MHD_Daemon *pDaemon;
  struct sigaction action;
  INETADDRESS address;
  poptContext context;
  char location [128], hostname [64];


  /*  Process the command-line arguments.
  */

  int port = 0, nThreads = 16, MaxAge = 0, timeout = 0, nMaxConns = 16;
  int fUseNumericAddrs = 0, fLocalOnly = 0;
  const char *pStylesheetFile = NULL, *pAddress = NULL;

  poptOption options []
         = {{"addr", 'a', POPT_ARG_STRING, &pAddress, 0,
             "Local IP address to use", "a.b.c.d"},
            {"port", 'p', POPT_ARG_INT, &port, 0,
          	 "Port on which to listen for client connections (1-65535)", "p"},
            {"localonly", 'l', POPT_ARG_NONE, &fLocalOnly, 0,
             "Allow connections only from this machine"
                " (equivalent to --addr=127.0.0.1)", NULL},
            {"numeric", 'n', POPT_ARG_NONE, &fUseNumericAddrs, 0,
             "Use numerical IP addresses instead of hostnames in URLs", NULL},
            {"timeout", '\0', POPT_ARG_INT, &timeout, 0,
             "Connections time out after n seconds of inactivity", "n"},
            {"maxconns", '\0', POPT_ARG_INT, &nMaxConns, 0,
             "Maximum number of concurrent connections", "n"},
          	{"threads", '\0', POPT_ARG_INT, &nThreads, 0,
             "Number of threads in the thread pool", "n"},
            {"max-age", '\0', POPT_ARG_INT, &MaxAge, 0,
             "Maximum time (in minutes) to keep pages in cache", "m"},
            {"syslog", '\0', POPT_ARG_NONE, &fUseSyslog, 0,
             "Write error and status information to the system log", NULL}, 
            {"stylesheet", 's', POPT_ARG_STRING, &pStylesheetFile, 0,
             "CSS stylesheet to use", "file"},
            {"fontdir", 'f', POPT_ARG_STRING, &pFontDirectory, 0,
             "Directory for font files (must be fully-qualified)", "path"},
            {"help", 'h', POPT_ARG_NONE, NULL, 100,
             "Show help (this message) and exit", NULL},
            {NULL, '\0', 0, NULL, 0, NULL, NULL}};


  context = poptGetContext (NULL, argc, (const char**) argv, options, 0);

  if ((t = poptGetNextOpt (context)) < -1)
  {
  	fprintf (stderr, "\nError: %s (%s)\n\n",
  		     poptStrerror (t),
  		     poptBadOption (context, 0));
  	return 1;
  }


  /*  If the --help or -h option was given, print the help information and
  *   quit.
  */

  if (t == 100)
  {
    printf ("%s\n", HelpIntro);
  	poptPrintHelp (context, stdout, 0);
  	printf ("\n");
  	return 0;
  }

  poptFreeContext (context);


  /*  Ensure that all given argument values are valid.
  */

  if ((port < 0) || (port > 65535))
  {
  	fprintf (stderr, "\nInvalid port number (must be 1..65535, inclusive).\n\n");
  	return 1;
  }

  if (MaxAge < 0)
  {
  	fprintf (stderr, "\nInvalid maximum age.\n\n");
  	return 1;
  }

  if (nThreads <= 0)
  {
  	fprintf (stderr, "\nInvalid number of threads.\n\n");
  	return 1;
  }

  if (timeout < 0)
  {
    fprintf (stderr, "\nInvalid timeout.\n\n");
    return 1;
  }

  if (nMaxConns <= 0)
  {
    fprintf (stderr, "\nInvalid number of connections.\n\n");
    return 1;
  }

  if (fLocalOnly)
  {
    IPAddress = htonl (INADDR_LOOPBACK);
  }
  else
  {
    if ((pAddress != NULL)
           && !inet_pton (AF_INET, pAddress, &IPAddress))
    {
      fprintf (stderr, "\nInvalid IP address: %s.\n\n", pAddress);
      return 1;
    }
  }

  if (pFontDirectory != NULL)
  {
    struct stat FileInfo;

    if (pFontDirectory [0] != '/')
    {
      fprintf (stderr, "\nFont directory must be a fully-qualified path.\n\n");
      return 1;
    }

    if ((stat (pFontDirectory, &FileInfo) != 0)
            || ((FileInfo.st_mode & S_IFMT) != S_IFDIR))
    {
      fprintf (stderr, "\nFont path\"%s\" does not exist or is not a directory.\n\n",
               pFontDirectory);
      return 1;
    }
  }


  /*  Initialize logging.
  */

  if (fUseSyslog)
  {
    openlog ("manhttp", LOG_PID, LOG_DAEMON);
  }


  /*  Create a socket and bind it to the specified IP address and port.
  */

  fdSocket = socket (AF_INET, SOCK_STREAM, 0);

  address.sin_family       = AF_INET;
  address.sin_port         = htons (port);
  address.sin_addr.s_addr  = IPAddress;

  if (bind (fdSocket, (const sockaddr*) &address, sizeof (INETADDRESS)))
  {
    int idError = errno;

    switch (idError)
    {
      case EADDRINUSE:
        if (port == 0)
        {
          ReportError ("All ephemeral ports are already in use.");
        }
        else
        {              
          ReportError ("The specified port (%d) is already in use.", port);
        }
        break;

      case EACCES:
        ReportError ("MANHTTP does not have the privilege level needed in order\n"
                     "to use the reserved ports (ports 1..1024, inclusive).");
        break;

      case EADDRNOTAVAIL:
        ReportError ("The specified IP address, %s, is not local to this machine.",
                     pAddress);
        break;

      default:
        ReportError ("Unable to bind to port %d.\n(Reason: %s)", 
                     port, strerror (idError));
    }  

    return 2;
  }

  listen (fdSocket, 64);


  /*  Call getsockname() to obtain the actual address and port number
  *   to which the socket is bound.
  */
  
  socklen_t cbAddress = sizeof (INETADDRESS);
  getsockname (fdSocket, (struct sockaddr*) &address, &cbAddress);

  port = ntohs (address.sin_port);

  if (address.sin_addr.s_addr == 0)
  {
    address.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
  }


  /*  Construct the URL prefix.
  */

  if (fLocalOnly)
  {
    strcpy (hostname, fUseNumericAddrs ? "127.0.0.1" : "localhost");  	
  }
  else if (fUseNumericAddrs)
  {
    inet_ntop (AF_INET, &address.sin_addr, hostname, sizeof (hostname));
  }
  else
  {  
    gethostname (hostname, sizeof (hostname));
  }

  snprintf (location, sizeof (location), "%s:%d", hostname, port);
  asprintf (&pUriPrefix, "http://%s/", location);


  /*  Load the stylesheet if one was specified.
  */

  if ((pStylesheetFile == NULL) 
         || (pStylesheetFile [0] == '\0'))
  {
    pStylesheet = DefaultStylesheet;
  }
  else
  {
    char *pText, *pError = NULL;
    int cbText;

    if (!LoadFile (pStylesheetFile, &pText, &cbText, &pError))
    {
      ReportError ("Unable to open \"%s\": %s", pStylesheetFile, pError);
      return 1;
    }

    pStylesheet = pText;
  }


  /*  Do miscellaneous initialization.
  */

  htmlInitializeRegexes ();
  manInitializeRegexes ();
  infoInitializeRegexes ();


  if (nThreads > MAX_THREADS)
  {
  	nThreads = MAX_THREADS;
  }

  if (MaxAge > 0)
  {
  	snprintf (CachePolicy, sizeof (CachePolicy),
              "private, max-age=%d", MaxAge * 60);
  }
  else
  {
  	strcpy (CachePolicy, "private");
  }


  chdir ("/");


  /*  Install a handler for the SIGINT signal.
  */

  memset (&action, 0, sizeof (struct sigaction));
  action.sa_sigaction   = (void (*) (int, siginfo_t*, void*))
                            [] (int, siginfo_t*, void*)  { fReadyToQuit = true; };                  
  action.sa_flags       = SA_SIGINFO;

  sigaction (SIGINT, &action, NULL);


  /*  Start the server.
  */

  pDaemon = MHD_start_daemon 
                  (MHD_USE_INTERNAL_POLLING_THREAD,
                   port,
                   NULL,
                   NULL,
                   HandleRequest,
                   NULL,
                   MHD_OPTION_LISTEN_SOCKET,
                   (MHD_socket) fdSocket,
                   MHD_OPTION_THREAD_POOL_SIZE,
                   nThreads,
                   MHD_OPTION_CONNECTION_TIMEOUT,
                   timeout,
                   MHD_OPTION_CONNECTION_LIMIT,
                   nMaxConns,
                   MHD_OPTION_END);


  /*  Terminate if the server couldn't be started.
  *   (This is unlikely.)
  */

  if (pDaemon == NULL)
  {
    ReportError ("Unable to start the HTTP server.");
    return 3;
  }


  printf ("MANHTTP is listening at %s.\n", location);

  if (fUseSyslog)
  {
    syslog (LOG_INFO, "MANHTTP is listening at %s.", location);
  }   


  /*  Sleep until a SIGINT signal comes along.
  */

  while (!fReadyToQuit)
  {
    sleep (3600);
  }


  printf ("\nReceived SIGINT.  Terminating...\n\n");


  /*  Shut down the server.
  */

  MHD_stop_daemon (pDaemon);

  return 0;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                              ReportError
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void ReportError
   (const char  *pFormatStr,
    ...)

{
  int i;
  char c, *pMessage;
  va_list args;


  va_start (args, pFormatStr);
  vasprintf (&pMessage, pFormatStr, args);
  va_end (args);

  
  fprintf (stderr, "\n%s\n\n", pMessage);


  if (fUseSyslog)
  {
    for (i = 0; (c = pMessage [i]) != '\0'; i++)
    {
      if ((c == '\n') || (c == '\r') || (c == '\t'))
      {
        pMessage [i] = ' ';
      }
    }

    syslog (LOG_ERR, "%s", pMessage);
  }


  free (pMessage);
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                            HandleRequest
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
int HandleRequest
   (void             *pClosureData, 
    MHD_Connection   *pConn, 
    const char       *pPath, 
    const char       *pMethod,     
    const char       *pProtocolVersion,
    const char       *pUploadData, 
    size_t           *pcbUploadData, 
    void            **ppContext)

{ 
  MHD_Response *pResp;

  static const char MethodError [] 
          = "Only the GET and HEAD HTTP methods are supported.";


  /*  Make sure that the HTTP method is GET or HEAD; if it is
  *   anything else (e.g., POST), return error 501.  
  */

  if ((strcasecmp (pMethod, "GET") != 0)
         && (strcasecmp (pMethod, "HEAD") != 0))
  {
    pResp = MHD_create_response_from_buffer 
                        (sizeof (MethodError), (void*) MethodError, 
                         MHD_RESPMEM_PERSISTENT);

    MHD_add_response_header (pResp, "Content-Type", "text/plain");
    MHD_queue_response (pConn, 501, pResp);
    MHD_destroy_response (pResp);
    return MHD_YES;
  }


  /*  Handle requests for font files.
  */

  if (memcmp (pPath, "/fonts/", 7) == 0)
  {
    HandleFontRequest (pConn, pPath);
    return MHD_YES;
  }


  /*  Handle requests for the page icon.
  */

  if (strcmp (pPath, "/favicon.ico") == 0)
  {
    pResp = MHD_create_response_from_buffer 
                        (sizeof (FavIcon), (void*) FavIcon, 
                         MHD_RESPMEM_PERSISTENT);

    MHD_add_response_header (pResp, "Content-Type", "image/x-icon");
    MHD_add_response_header (pResp, "Cache-Control", CachePolicy);    
    MHD_queue_response (pConn, 200, pResp);
    MHD_destroy_response (pResp);
    return MHD_YES;
  }


  /*  Handle manual page requests.
  */

  if (memcmp (pPath, "/man", 4) == 0)
  {
    HandleManPageRequest (pConn, pPath);
    return MHD_YES;
  }


  /*  Handle info requests.
  */

  if (memcmp (pPath, "/info", 5) == 0)
  {
    HandleInfoRequest (pConn, pPath);
    return MHD_YES;
  }


  /*  Handle apropos requests.
  */

  if (memcmp (pPath, "/apropos", 8) == 0)
  {
    HandleAproposRequest (pConn, pPath);
    return MHD_YES;
  }


  /*  If we've reached this point, the URL isn't a recognized one,
  *   Display the MANHTTP splash page.
  */

  GenerateSplashPage (pConn, pPath);
  return MHD_YES;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                        HandleFontRequest
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void HandleFontRequest
   (MHD_Connection  *pConn,
    const char      *pPath)

{
  int fd = -1, status;
  const char *pTypeStr;
  const char *pFilename = pPath + 7;
  char *pFontPath;
  struct MHD_Response *pResp;
  struct stat FileInfo;


  if ((pFontDirectory != NULL)
         && ((pTypeStr = FontTypeFromFilename (pFilename)) != NULL))
  {
    asprintf (&pFontPath, "%s/%s", pFontDirectory, pFilename);

    if ((fd = open (pFontPath, O_RDONLY | O_CLOEXEC)) >= 0)
    {
      fstat (fd, &FileInfo);
      if (((FileInfo.st_mode & S_IFMT) != S_IFREG)
             || (FileInfo.st_size == 0))
      {
        close (fd);
        fd = -1;
      }     
    }

    free (pFontPath);
  }

 
  if (fd >= 0)
  {
    pResp = MHD_create_response_from_fd_at_offset64 (FileInfo.st_size, fd, 0);
    MHD_add_response_header (pResp, "Cache-Control", CachePolicy);

    status = 200;                    
  }
  else
  {
    pResp = MHD_create_response_from_buffer
                    (20, (void*) "Font file not found.", MHD_RESPMEM_PERSISTENT);

    pTypeStr = "text/plain";
    status = 404;
  }  

  MHD_add_response_header (pResp, "Content-Type", pTypeStr);
  MHD_queue_response (pConn, status, pResp);
  MHD_destroy_response (pResp);
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                     FontTypeFromFilename
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
const char* FontTypeFromFilename
   (const char   *pFilename)

{
  int i, t;
  const char *pExtension, *pType;

  static const char AllowedChars []
        = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._- ";

  static const char *TypeList []
            = {"ttf",    "font/ttf",
               "otf",    "font/otf",
               "woff",   "font/woff",
               "woff2",  "font/woff2",
               "eot",    "application/vnd.ms-fontobject",
               NULL};


  t = strspn (pFilename, AllowedChars);
  if (pFilename [t] != '\0')
    return NULL;


  if ((pExtension = strrchr (pFilename, '.')) == NULL)
    return NULL;

  pExtension++;

  i = 0;
  while (((pType = TypeList [i]) != NULL)
             && (strcasecmp (pExtension, pType) != 0))
  {
    i += 2;
  }

  return (pType == NULL) ? NULL : TypeList [i + 1];
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                     HandleManPageRequest
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void HandleManPageRequest
   (MHD_Connection  *pConn,
    const char      *pPath)

{
  int cbPageContent;
  bool fSuccess;
  size_t cbResponse = 0;
  char *pPageContent, *pResponse = NULL;
  FILE *stream;
  struct MHD_Response *pResp;
  PROCESSERRORINFO error;
  char page [64], section [8], CanonicalID [80];
 

  /*  Parse the title into the page name and section components.
  */  

  if ((pPath [4] != '/')
         || !ParseManPageTitle (pPath + 5, page, sizeof (page), section, sizeof (section)))
  {
    GenerateErrorPage (pConn, "Invalid", 400, "Invalid manual page specification.");
    return;
  }


  /*  Construct the page's canonical ID.
  */
  
  snprintf (CanonicalID, sizeof (CanonicalID),
            ((section [0] == '\0') ? "%s" : "%s(%s)"),
            page, section);


  /*  Obtain the raw manual page text.
  */

  fSuccess = GetManPageContent 
                 (ManPath, page, section, &pPageContent,
                  &cbPageContent, &error);


  if (!fSuccess)
  {
    if ((error.context == ERRORCTXT_OTHER)
    	     && !WIFSIGNALED (error.ErrorCode))
    {
      GenerateErrorPage 
              (pConn, "Not found", 404,
               "No manual page is available for &ldquo;%s&rdquo;.",
               CanonicalID);
    }
    else
    {
      HandleInternalError (pConn, ManPath, &error);
    }

  	return;
  }


  stream = open_memstream (&pResponse, &cbResponse);
  ManualPageToHTML (stream, CanonicalID, pUriPrefix, pStylesheet, 
                    pPageContent, cbPageContent);
  fclose (stream);

  free (pPageContent);


  pResp = MHD_create_response_from_buffer
                (cbResponse, pResponse, MHD_RESPMEM_MUST_FREE);
    
  MHD_add_response_header (pResp, "Content-Type", "text/html");
  MHD_add_response_header (pResp, "Cache-Control", CachePolicy);
  MHD_queue_response (pConn, 200, pResp);
  MHD_destroy_response (pResp);
} 



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                        HandleInfoRequest
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void HandleInfoRequest
   (MHD_Connection  *pConn,
    const char      *pPath)

{
  int result, cbContent;
  size_t cbResponse = 0;
  char *pResponse = NULL, *pRedirectUri, *pFile;
  char *pNodeName, *pContent, *pDecodedName;
  FILE *stream;
  struct MHD_Response *pResp;
  PROCESSERRORINFO error;
  char keyword [128];


  if ((pPath [5] != '/') && (pPath [5] != '\0'))
  {
    GenerateErrorPage 
          (pConn, "Invalid", 400, "Invalid info page specification.");
    return;
  }


  if ((pPath [5] == '\0')
         || (NormalizeSpaces (pPath + 6, keyword, sizeof (keyword)) == 0))
  {
    strcpy (keyword, "dir/(dir)");
  }


  if ((pNodeName = strchr (keyword, '/')) != NULL)
  {
    *(pNodeName++) = '\0';
    pDecodedName = DecodeInfoNodeName (pNodeName, -1);


    if (!GetInfoContent (InfoPath, keyword, pDecodedName, 
                         &pContent, &cbContent, &error))
    {
      free (pDecodedName);
      HandleInternalError (pConn, InfoPath, &error);
      return;    	
    }


    if (pContent == NULL)
    {
      GenerateErrorPage 
               (pConn, "Node not found", 404,
                "The Info file <span class=\"Filename\">%s</span> contains"
                " no node with the name &ldquo;%s&rdquo;.",
                keyword, pDecodedName);

      free (pDecodedName);
      return;     
    }


    stream = open_memstream (&pResponse, &cbResponse);
    InfoToHTML (stream, keyword, pDecodedName, pUriPrefix, pStylesheet,
                pContent, cbContent);
    fclose (stream);

    pResp = MHD_create_response_from_buffer
                (cbResponse, pResponse, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header (pResp, "Content-Type", "text/html");
    MHD_add_response_header (pResp, "Cache-Control", CachePolicy);
    MHD_queue_response (pConn, 200, pResp);
    MHD_destroy_response (pResp);

    free (pContent);    
    free (pDecodedName);
    return;
  }
    

  result = InfoFileFromKeyword (InfoPath, keyword, &pFile);
  
  switch (result)
  {
    case INFO_SUCCESS:
      asprintf (&pRedirectUri, "%sinfo/%s/%s", 
                pUriPrefix, 
                pFile, 
                ((strcasecmp (pFile, keyword) == 0) ? "Top" : keyword));

      pResp = MHD_create_response_from_buffer
                    (14, (void*) "Redirecting...", MHD_RESPMEM_PERSISTENT);
      MHD_add_response_header (pResp, "Content-Type", "text/plain");
      MHD_add_response_header (pResp, "Location", pRedirectUri);
      MHD_queue_response (pConn, 301, pResp);
      MHD_destroy_response (pResp);

      free (pRedirectUri);
      break;

    case INFO_REDIRECT_TO_MAN_PAGE:
      asprintf (&pRedirectUri, "%sman/%s", 
                pUriPrefix, 
                keyword);

      pResp = MHD_create_response_from_buffer
                    (29, (void*) "Redirecting to manual page...", 
                     MHD_RESPMEM_PERSISTENT);
      MHD_add_response_header (pResp, "Content-Type", "text/plain");
      MHD_add_response_header (pResp, "Location", pRedirectUri);
      MHD_queue_response (pConn, 301, pResp);
      MHD_destroy_response (pResp);

      free (pRedirectUri);
      break;

    case INFO_NOT_FOUND:
      GenerateErrorPage 
           (pConn, "Not found", 404,
            "No Info file found for &ldquo;%s&rdquo;.",
            keyword);
      break;
  }

  free (pFile);
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                     HandleAproposRequest
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void HandleAproposRequest
   (MHD_Connection  *pConn,
    const char      *pPath)

{
  int nResults = 0;
  bool fSuccess;
  size_t cbResponse = 0;
  char *pResponse = NULL;
  APROPOSRESULT *pResultList = NULL;
  FILE *stream;
  struct MHD_Response *pResp;
  char keyword [80];
  PROCESSERRORINFO error;


#if defined(TARGET_BSD)
  static const char RebuildCommand [] = "makewhatis(8)";
#else
  static const char RebuildCommand [] = "mandb(8)";
#endif


  if ((pPath [8] != '/')
         || (NormalizeSpaces (pPath + 9, keyword, sizeof (keyword)) == 0))
  {
    GenerateErrorPage (pConn, "Invalid", 400, "Invalid apropos request.");
    return;
  }


  fSuccess = GetAproposContent (AproposPath, keyword, &pResultList, &nResults, &error);


  if (!fSuccess)
  {
    if ((error.context == ERRORCTXT_OTHER)
   	        && !WIFSIGNALED (error.ErrorCode))
    {	
      GenerateErrorPage 
           (pConn, "Nothing found", 404,
            "Apropos search for &ldquo;%s&rdquo; returned no results.\n"
            "<div style=\"height: 1.5em\"></div>\n"
            "If you keep getting this message, it is likely that the\n"
            "system's manual page index needs to be updated.  You\n"
            "(or the system administrator) can do this by running\n"
            "<a href=\"man/%s\">%s</a>.\n",
            keyword,
            RebuildCommand,
            RebuildCommand);
    }
    else
    {
      HandleInternalError (pConn, AproposPath, &error);
    }

    return;    	
  }


  stream = open_memstream (&pResponse, &cbResponse);
  AproposResultsToHTML (stream, keyword, pUriPrefix, pStylesheet, pResultList, nResults);
  fclose (stream);

  free (pResultList);


  pResp = MHD_create_response_from_buffer
              (cbResponse, pResponse, MHD_RESPMEM_MUST_FREE);

  MHD_add_response_header (pResp, "Content-Type", "text/html");
  MHD_add_response_header (pResp, "Cache-Control", CachePolicy);
  MHD_queue_response (pConn, 200, pResp);
  MHD_destroy_response (pResp);
} 



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                       GenerateSplashPage
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void GenerateSplashPage
   (MHD_Connection  *pConn,
    const char      *pPath)

{
  int status;
  size_t cbResponse = 0;
  char *pResponse = NULL;
  struct MHD_Response *pResp;
  FILE *stream;


  stream = open_memstream (&pResponse, &cbResponse);

  fprintf (stream, 
           "<!DOCTYPE html>\n\n"
           "<html>\n"
           "<head>\n"
           "<title>MANHTTP</title>\n"
           "<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n"
           "<base href=\"%s\">\n"           
           "<style>\n%s\n</style>\n"
           "</head>\n"
           "<body Type=\"splash\">\n"
           "%s"
           "</body>\n"
           "</html>\n",
           pUriPrefix,
           pStylesheet,
           SplashText);

  fclose (stream);


  status = ((pPath [0] == '\0') || (strcmp (pPath, "/") == 0))
               ? 200 : 404;

  pResp = MHD_create_response_from_buffer
              (cbResponse, pResponse, MHD_RESPMEM_MUST_FREE);

  MHD_add_response_header (pResp, "Content-Type", "text/html");
  MHD_add_response_header (pResp, "Cache-Control", CachePolicy);
  MHD_queue_response (pConn, status, pResp);
  MHD_destroy_response (pResp);
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                        GenerateErrorPage
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void GenerateErrorPage 
   (MHD_Connection  *pConn,
    const char      *pErrorType,
    int              HttpStatus,
    const char      *pFormatStr, 
    ...)

{
  size_t cbResponse = 0;
  char *pResponse = NULL;
  FILE *stream;
  struct MHD_Response *pResp;
  char *pMessage;
  va_list args;


  stream = open_memstream (&pResponse, &cbResponse);

  fprintf (stream, 
           "<!DOCTYPE html>\n\n"
           "<html>\n"
           "<head>\n"
           "<title>%s</title>\n"
           "<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n"
           "<base href=\"%s\">\n"                      
           "<style>\n%s\n</style>\n"
           "</head>\n"
           "<body Type=\"splash\" ErrorPage=\"\">\n",
           pErrorType,
           pUriPrefix,
           pStylesheet);


  fprintf (stream, "<div class=\"Splash\">\n");

  fprintf (stream, "<p Heading=\"\">%s</p>\n", pErrorType);

  if (strchr (pFormatStr, '%') == NULL)
  {
    fprintf (stream, "%s\n", pFormatStr);
  }
  else
  {
    va_start (args, pFormatStr);
    vasprintf (&pMessage, pFormatStr, args);
    va_end (args);
  
    fprintf (stream, "%s\n", pMessage);
    free (pMessage);
  }


  fprintf (stream, 
           "<p style=\"font-size: 75%%; margin-top: 40px; margin-bottom: 3px;\">\n"
           "<a href=\"/\">MANHTTP home</a>\n"
           "</p>\n"
           "</div>\n</body>\n</html>\n");

  fclose (stream);


  pResp = MHD_create_response_from_buffer
              (cbResponse, pResponse, MHD_RESPMEM_MUST_FREE);

  MHD_add_response_header (pResp, "Content-Type", "text/html");
  MHD_add_response_header (pResp, "Cache-Control", CachePolicy);
  MHD_queue_response (pConn, HttpStatus, pResp);
  MHD_destroy_response (pResp);
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                      HandleInternalError
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void HandleInternalError
   (MHD_Connection          *pConn,
    const char              *pCommandPath,
    const PROCESSERRORINFO  *pError)

{
  int cbMax;
  size_t cbResponse = 0;
  char *pResponse = NULL, *pErrorHTML = NULL;
  const char *pCommand, *pMessage;
  FILE *stream;
  struct MHD_Response *pResp;
  char buffer [256] = "";


  stream = open_memstream (&pResponse, &cbResponse);

  fprintf (stream, 
           "<!DOCTYPE html>\n\n"
           "<html>\n"
           "<head>\n"
           "<title>MANHTTP Internal Error</title>\n"
           "<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n"
           "<base href=\"%s\">\n"                      
           "<style>\n%s\n</style>\n"
           "</head>\n"
           "<body Type=\"splash\" ErrorPage=\"\">\n",
           pUriPrefix,
           pStylesheet);


  fprintf (stream, 
           "<div class=\"Splash\">\n"
           "<p Heading=\"\">Internal error</p>\n");


  pCommand = strrchr (pCommandPath, '/');
  pCommand = (pCommand == NULL) ? pCommandPath : (pCommand + 1);

  fprintf (stream,
           "MANHTTP encountered an unrecoverable error when running\n"
           "<span class=\"Filename\">%s</span>.  Detailed error information\n"
           "follows.\n<br>\n<br>\n",
           pCommand);


  if ((pError->context == ERRORCTXT_FORK_FAILED)
         || (pError->context == ERRORCTXT_EXEC_FAILED))
  {
    pMessage = strerror_r (pError->ErrorCode, buffer, sizeof (buffer));
    if ((pMessage == NULL) || (pMessage [0] == '\0'))
    {
      snprintf (buffer, sizeof (buffer),
                "(No error information was provided; error code=%d.)",
                 pError->ErrorCode);
      pMessage = buffer;
    }

    cbMax = strlen (pMessage) * 5 + 1;
    pErrorHTML = (char*) malloc (cbMax);
    HTMLizeText (pErrorHTML, cbMax, pMessage, -1, NULL, NULL, 0);


    if (pError->context == ERRORCTXT_EXEC_FAILED)
    {
      fprintf (stream,
               "Unable to execute <span class=\"Filename\">%s</span>.<br>\n"
               "%s\n",
               pCommandPath,
               pErrorHTML);
    }
    else
    {
      fprintf (stream,
               "Unable to create a new process.<br>\n"
               "%s\n",
               pErrorHTML);
    }

    free (pErrorHTML);
  }
  else
  {
    if (WIFSIGNALED (pError->ErrorCode))
    {
      fprintf (stream,
               "Process was terminated unexpectedly.<br>\n"
               "Signal ID: %d\n",
               WTERMSIG (pError->ErrorCode));
    }
  }


  fprintf (stream, 
           "<p style=\"font-size: 75%%; margin-top: 40px; margin-bottom: 3px;\">\n"
           "<a href=\"/\">MANHTTP home</a>\n"
           "</p>\n"
           "</div>\n</body>\n</html>\n");

  fclose (stream);


  pResp = MHD_create_response_from_buffer
              (cbResponse, pResponse, MHD_RESPMEM_MUST_FREE);

  MHD_add_response_header (pResp, "Content-Type", "text/html");
  MHD_add_response_header (pResp, "Cache-Control", CachePolicy);
  MHD_queue_response (pConn, 500, pResp);
  MHD_destroy_response (pResp);  
}
