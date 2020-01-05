/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
  manhttp 

  An HTTP server that provides a web-based, hypertext-driven
  frontend for man(1), info(1), and apropos(1).  MANHTTP lets you
  browse and search your system's online documentation using a web
  browser.

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

-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/


#include <stdlib.h>                    /*  C/C++ RTL headers.  */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <limits.h>
#include <envz.h>

#include "utility.h"                  /*  Application headers.  */



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                          EllipsizeString
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

bool EllipsizeString
   (const char   *pSourceStr,
    char         *pDestStr,
    int           cbMax,
    int           nMaxChars)

{
  int i, cb, nChars;
  char c;


  for (i = nChars = 0; (c = pSourceStr [i]) != '\0'; i++)
  {
    if (((c & 0xc0) != 0x80) && (++nChars == nMaxChars))
      break;
  }

  cb = i;


  if (nChars < nMaxChars)
  {
    if (pSourceStr != pDestStr)
    {
      strncpy (pDestStr, pSourceStr, cbMax);
    }

    return false;
  }


  if (pSourceStr == pDestStr)
  {
    strcpy (pDestStr + cb, "..."); 
  }
  else
  {
    snprintf (pDestStr, cbMax, "%.*s...", cb, pSourceStr);
  }

  return true;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                                 LoadFile
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

bool LoadFile
   (const char   *pPath,
    char        **ppContentOut,
    int          *pcbOut,
    char        **ppErrorOut)

{
  int fd, cbFile;
  char *pContent;
  struct stat status;


  *ppContentOut = NULL;
  *pcbOut = 0;  


  if ((fd = open (pPath, O_RDONLY | O_CLOEXEC)) < 0)
  {
    *ppErrorOut = strdup (strerror (errno));
    return false;
  }


  fstat (fd, &status);

  if ((status.st_mode & S_IFMT) != S_IFREG)
  {
    close (fd);
    *ppErrorOut = strdup ("Not a regular file");
    return false;
  }


  cbFile = status.st_size;
  pContent = (char*) malloc (cbFile + 1);
  read (fd, pContent, cbFile);
  close (fd);


  pContent [cbFile] = '\0';

  *ppContentOut = pContent;
  *pcbOut = cbFile;
  *ppErrorOut = NULL;

  return true;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                          NormalizeSpaces
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

int NormalizeSpaces 
   (const char  *pStr, 
    char        *pDest,
    int          cbMax)

{
  int i, j;
  char c;
  bool fSpace;


  for (i = j = 0, fSpace = false; (c = pStr [i]) != '\0'; i++)
  {
    if ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r'))
    {
      fSpace = true;
    }
    else
    {
      if (cbMax - j < (fSpace ? 3 : 2))
        break;

      if (fSpace && (j > 0))
      {
        pDest [j++] = ' ';        
      }

      pDest [j++] = c;
      fSpace = false;  
    }
  }

  pDest [j] = '\0';

  return j;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                           JSEscapeString
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

bool JSEscapeString 
   (const char  *pStrSource, 
    char        *pStrDest, 
    int          cbDestMax)

{
  int i, n, cbEscape;
  unsigned int CodePoint, c2, c3, c4;
  unsigned char c;
  char d;
  bool fTruncated = false;

  static const char HexDigits [] = "0123456789abcdef";


  i = n = 0;
  while ((c = (unsigned char) pStrSource [i++]) != '\0')
  {
    if ((c >= 32) && (c <= 126) 
           && (c != '\\') && (c != '"'))
    {
      if (cbDestMax - n < 2)
      {
        fTruncated = true;
        break;
      }

      pStrDest [n++] = (char) c;
      continue;
    }

    switch (c)
    {
      case '"':   d = '"';   cbEscape = 2;  break;
      case '\\':  d = '\\';  cbEscape = 2;  break;
      case '\t':  d = 't';   cbEscape = 2;  break;
      case '\n':  d = 'n';   cbEscape = 2;  break;
      case '\r':  d = 'r';   cbEscape = 2;  break;
      default:    d = 'u';   cbEscape = 6;  
    }  

    if (cbDestMax - n < cbEscape + 1)
    {
      fTruncated = true;
      break;
    }

    pStrDest [n++] = '\\';
    pStrDest [n++] = d;

    if (d == 'u') 
    {
      if ((c & 0x80) == 0)
      {
        CodePoint = (unsigned int) c;
      }
      else if ((c & 0xe0) == 0xc0)
      {
        c2 = (unsigned int) (pStrSource [i++] & 0x3f);
        CodePoint = ((c & 0x1f) << 6) | c2;
      }
      else if ((c & 0xf0) == 0xe0)
      {
        c2 = (unsigned int) (pStrSource [i++] & 0x3f);
        c3 = (unsigned int) (pStrSource [i++] & 0x3f);
        CodePoint = ((c & 0x0f) << 12) | (c2 << 6) | c3;
      }
      else if ((c & 0xf8) == 0xf0)
      {
        c2 = (unsigned int) (pStrSource [i++] & 0x3f);
        c3 = (unsigned int) (pStrSource [i++] & 0x3f);
        c4 = (unsigned int) (pStrSource [i++] & 0x3f);
        CodePoint = ((c & 0x03) << 18) | (c2 << 12) | (c3 << 6) | c4;
      }
      else
      {
        /*  Invalid UTF-8 sequence--skip it.  */
        continue;
      }

      pStrDest [n++] = HexDigits [(CodePoint & 0xF000) >> 12];
      pStrDest [n++] = HexDigits [(CodePoint & 0x0F00) >> 8];        
      pStrDest [n++] = HexDigits [(CodePoint & 0x00F0) >> 4];
      pStrDest [n++] = HexDigits [CodePoint & 0x000F];  
    }
  }

  pStrDest [n] = '\0';

  return fTruncated;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                           CountCharsUTF8
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

int CountCharsUTF8
  (const char  *pStr,
   int          cbStr)

{
  int i, count;
  char c;


  if (cbStr < 0)
  {
    cbStr = strlen (pStr);
  }


  for (i = count = 0; i < cbStr; i++)
  {
  	if ((pStr [i] & 0xC0) != 0x80)
  	{
  	  count++;
  	}
  }

  return count;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                           SetCloseOnExec
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

void SetCloseOnExec
   (int   fd,
    bool  fCloseOnExec)

{
  int OldFlags, NewFlags;


  if (fd < 0)
    return;

  OldFlags = fcntl (fd, F_GETFD, 0);

  NewFlags = fCloseOnExec
                ? (OldFlags | FD_CLOEXEC)
                : (OldFlags & ~FD_CLOEXEC);

  if (NewFlags != OldFlags)
  {
    fcntl (fd, F_SETFD, NewFlags);
  }
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                       CreateChildProcess
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

bool CreateChildProcess
    (pid_t          *pidOut,     
     int            *pResultOut,   
     const char     *pszExecutable,
     const char    **ppszArguments,
     int             flags,
     int            *pfdStdInput,
     int            *pfdStdOutput,
     int            *pfdStdError)

{
  int result = 0, fdInput = -1, fdOutput = -1, fdError = -1;
  int fdInputRedir = -1, fdOutputRedir = -1, fdErrorRedir = -1;
  int fdPipe [2], fdFailurePipe [2];
  bool fSuccess = true;
  pid_t idChild;
  struct pollfd pfd;

  extern char **environ;


  if (flags & STDIN_REDIRECT)
  {
    pipe (fdPipe);
    fdInput = fdPipe [1];
    fdInputRedir = fdPipe [0];
  }
  else if (flags & STDIN_NULL)
  {
    fdInputRedir = open ("/dev/null", O_RDONLY);
  }


  if (flags & STDOUT_REDIRECT)
  {
    pipe (fdPipe);
    fdOutput = fdPipe [0];
    fdOutputRedir = fdPipe [1];
  }
  else if (flags & STDOUT_NULL)
  {
    fdOutputRedir = open ("/dev/null", O_WRONLY);
  }


  if (flags & STDERR_REDIRECT)
  {
    pipe (fdPipe);
    fdError = fdPipe [0];
    fdErrorRedir = fdPipe [1];
  }
  else if (flags & STDERR_NULL)
  {
    fdErrorRedir = open ("/dev/null", O_WRONLY);
  }


  /*  Create a pipe for reporting an execve() failure to the parent process.
  */

  pipe (fdFailurePipe);


  SetCloseOnExec (fdFailurePipe [1], true);
  SetCloseOnExec (fdInput, true);
  SetCloseOnExec (fdOutput, true);
  SetCloseOnExec (fdError, true);


  if ((idChild = fork ()) < 0)
  {
    result = errno;
    fSuccess = false;
  }


  if (idChild == 0)
  {
    close (fdFailurePipe [0]);


    if (fdInputRedir >= 0)
    {
      dup2 (fdInputRedir, 0);
      close (fdInputRedir);
    }

    if (fdOutputRedir >= 0)
    {
      dup2 (fdOutputRedir, 1);
      close (fdOutputRedir);
    }

    if (fdErrorRedir >= 0)
    {
      dup2 (fdErrorRedir, 2);
      close (fdErrorRedir);
    }


    execve (pszExecutable, (char* const*) ppszArguments, environ);


    /*  If we reach this point, something must have gone wrong.
    *   Report the failure to the parent process, then terminate.
    */

    int ExecveResult = errno;
    write (fdFailurePipe [1], &ExecveResult, sizeof (int));

    exit (255);
  }


  /*   Close the file descriptors that are not needed in the parent process.
  */

  close (fdFailurePipe [1]);

  if (fdInputRedir >= 0)
  {
    close (fdInputRedir);
  }

  if (fdOutputRedir >= 0)
  {
    close (fdOutputRedir);
  }

  if (fdErrorRedir >= 0)
  {
    close (fdErrorRedir);
  }


  /*  Wait until the failure-report pipe is closed or some data appears
  *   in it.
  */

  if (fSuccess)
  {
    pfd.fd       = fdFailurePipe [0];
    pfd.events   = POLLIN;
    pfd.revents  = 0;
    poll (&pfd, 1, -1);

    if (pfd.revents & POLLIN)
    {
      read (fdFailurePipe [0], &result, sizeof (int));
      waitpid (idChild, NULL, 0);
      fSuccess = false;
    }
  }


  close (fdFailurePipe [0]);


  if (!fSuccess)
  {
    if (fdInput >= 0)
    {
      close (fdInput);
    }

    if (fdOutput >= 0)
    {
      close (fdOutput);
    }

    if (fdError >= 0)
    {
      close (fdError);
    }

    fdInput = fdOutput = fdError = -1;
    idChild = -1;
  }
 

  *pidOut = idChild;

  if (pResultOut != NULL)
  {
    *pResultOut = result;
  }

  if (pfdStdInput != NULL)
  {
    *pfdStdInput = fdInput;
  }

  if (pfdStdOutput != NULL)
  {
    *pfdStdOutput = fdOutput;
  }

  if (pfdStdError != NULL)
  {
    *pfdStdError = fdError;
  }

  return fSuccess;
}
 


/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                             CaptureInput
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

int CaptureInput
   (int      fd,
    void   **ppDataOut,
    int     *pcbDataOut,
    int      cbMaxData,
    char     cZeroReplace)

{
  int i, cbRead, cbReadSoFar, cbBuffer, cbExtra;
  char *pBuffer;
  struct pollfd pfd;


  /*  Allocate a buffer.
  */

  cbRead = cbReadSoFar = 0;
  cbBuffer = (cbMaxData <= 0) ? 128 : (cbMaxData + 1);
  pBuffer = (char*) malloc (cbBuffer);


  /*  Read data into the buffer until (1) an error or zero-length read
  *   occurs, (2) cbMaxData bytes have been read, or (3) poll() reports
  *   a hang-up or error condition on the file descriptor.
  */

  for (;;)
  {
    pfd.fd       = fd;
    pfd.events   = POLLIN;
    pfd.revents  = 0;
    poll (&pfd, 1, -1);
    
    if (pfd.revents & POLLIN)
    {
      if ((cbMaxData <= 0) && (cbReadSoFar >= cbBuffer - 1))
      {
        cbBuffer *= 2;
        pBuffer = (char*) realloc (pBuffer, cbBuffer);
      }

      if ((cbRead = read (fd, pBuffer + cbReadSoFar, cbBuffer - cbReadSoFar)) <= 0)
        break;

      cbReadSoFar += cbRead;

      if ((cbMaxData > 0) && (cbReadSoFar >= cbMaxData))
        break;
    }

    if (pfd.revents & (POLLHUP | POLLERR))
      break;
  }


  /*  Replace all zero characters with the specified replacement character.
  */

  if (cZeroReplace != '\0')
  {
    for (i = 0; i < cbReadSoFar; i++)
    {
      if (pBuffer [i] == '\0')
      {
        pBuffer [i] = cZeroReplace;
      }
    }
  }


  /*  Resize the buffer if necessary.
  */

  cbExtra = cbBuffer - cbReadSoFar;
  if ((cbExtra >= 64) || (cbExtra == 0))
  {
    pBuffer = (char*) realloc (pBuffer, cbReadSoFar + 1);
    cbExtra = 1;
  }

  memset (pBuffer + cbReadSoFar, 0, cbExtra);

  *ppDataOut = pBuffer;
  *pcbDataOut = cbReadSoFar;
  return 1;
}

