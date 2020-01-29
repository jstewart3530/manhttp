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
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>

#include <tre/tre.h>                   /*  Library headers.  */

#include "utility.h"                   /*  Application headers.  */
#include "installation.h"
#include "documentation_api.h"



static regex_t ParseRegex, AproposRegex;



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                                      Min
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static inline
int Min
   (int  a,
    int  b)

{
  return (a < b) ? a : b;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                     manInitializeRegexes
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

void manInitializeRegexes
   (void)

{
  int error;


  error = tre_regcomp 
             (&ParseRegex, 
              "^[[:space:]]*([[:alnum:]._:@+-]+)[[:space:]]*(\\([0-9]{0,3}[a-z]{0,4}\\))?[[:space:]]*$", 
              REG_EXTENDED | REG_ICASE | REG_NEWLINE);

  if (error != 0)
  {
    fprintf (stderr, "FATAL ERROR: regcomp() failed for ParseRegex.  (Error %d)\n", 
             error);
    exit (100);
  }


  error = tre_regcomp 
             (&AproposRegex, 
              "^[[:space:]]*([[:alnum:]._:@+-]+)[[:space:]]*\\(([0-9a-z/]+)\\)[[:space:]]*-[[:space:]]*(.+)[[:space:]]*", 
              REG_EXTENDED | REG_ICASE | REG_NEWLINE);

  if (error != 0)
  {
    fprintf (stderr, "FATAL ERROR: regcomp() failed for AproposRegex.  (Error %d)\n", 
             error);
    exit (100);
  }
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                        ParseManPageTitle
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

bool ParseManPageTitle
   (const char   *pStr,
    char         *pTitleOut,
    int           cbTitleMax,
    char         *pSectionOut,
    int           cbSectionMax)

{
  int length;
  regmatch_t match [3];

 
  if (tre_regexec (&ParseRegex, pStr, 3, match, 0))
    return false;
  

  if ((pTitleOut != NULL) && (cbTitleMax > 0))
  {
    length = Min (match [1].rm_eo - match [1].rm_so, cbTitleMax - 1);
    memcpy (pTitleOut, pStr + match [1].rm_so, length);
    pTitleOut [length] = '\0';
  }


  if ((pSectionOut != NULL) && (cbSectionMax > 0))
  {
    length = 0;
    if ((match [2].rm_so >= 0)
          && (match [2].rm_eo >= 0)
          && (match [2].rm_so < match [2].rm_eo)
          && (pStr [match [2].rm_so] == '(')
          && (pStr [match [2].rm_eo - 1] == ')'))
    {
      length = Min (match [2].rm_eo - match [2].rm_so - 2, cbSectionMax - 1);      
    }

    memcpy (pSectionOut, pStr + match [2].rm_so + 1, length);
    pSectionOut [length] = '\0';
  }

  return true;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                        GetManPageContent
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

bool GetManPageContent
   (const char          *pPageTitle,
    const char          *pSection,
    char               **ppDataOut,
    int                 *pcbDataOut,
    PROCESSERRORINFO    *pErrorOut)

{
  int nArgs = 0, fdOutput, status = 0, cbData;
  pid_t pid;
  char *pData;
  const char *pCommand, *pArguments [8];

  const char *pExecutable = ManPath;

  *ppDataOut = NULL;
  *pcbDataOut = 0;

 
  /*  Construct the argument list.
  */

  pArguments [nArgs++] = ((pCommand = strrchr (pExecutable, '/')) == NULL)
                             ? pExecutable
                             : (pCommand + 1);

  pArguments [nArgs++] = "--encoding=UTF-8";

  if ((pSection != NULL) && (pSection [0] != '\0'))
  {
    pArguments [nArgs++] = pSection;
  }

  pArguments [nArgs++] = pPageTitle;
  pArguments [nArgs] = NULL;  


  /*  Set environment variables that man(1) needs.
  */

  setenv ("TERM", "xterm-256color", true);
  setenv ("MAN_KEEP_FORMATTING", "yes", true);
  setenv ("MANWIDTH", "80", true);


  /*  Run man(1) as a child process.
  */

  if (!CreateChildProcess (&pid, pErrorOut, pExecutable, pArguments,
                           STDIN_NULL | STDOUT_REDIRECT | STDERR_NULL, 
                           NULL, &fdOutput, NULL))
    return false;


  /*  Capture the output from man(1) into a buffer.
  */

  CaptureInput (fdOutput, (void**) &pData, &cbData, 0, ' ');
  close (fdOutput);


  /*  Wait until the man(1) process has terminated; obtain its exit status.
  */

  waitpid (pid, &status, 0);


  if (WIFSIGNALED (status) || (WEXITSTATUS (status) != 0))
  {
    free (pData);

    pErrorOut->context    = ERRORCTXT_RUNTIME;
    pErrorOut->ErrorCode  = status;
    pErrorOut->pExecPath  = pExecutable;

    return false;
  }


  *ppDataOut = pData;
  *pcbDataOut = cbData;

  return true;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                        GetAproposContent
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

bool GetAproposContent
   (const char          *pSearchKeyword,
    APROPOSMODE          SearchMode,
    APROPOSRESULT      **ppResultsOut,
    int                 *pnResultsOut,
    PROCESSERRORINFO    *pErrorOut)

{
  int i, n, cb, status = 0, fdOutput;
  int cbRemaining, nLines, cbRawData, cbTotal, length;
  char *pRawData, *pLine, *pNextLine;
  char *pStr, *pPageTitle, *pSection, *pDescription;
  const char *pCommand, *pRawDataEnd, *pArguments [8];
  APROPOSRESULT *pResults;
  pid_t pid;
  regmatch_t match [4];

  const char *pExecutable = AproposPath;


  *ppResultsOut = NULL;
  *pnResultsOut = 0;


  /*  Build an argument list for apropos(1).
  */  

  pCommand = strrchr (pExecutable, '/');
  pCommand = (pCommand == NULL) ? pExecutable : (pCommand + 1);


  n = 0;
  pArguments [n++] = pCommand;
  pArguments [n++] = "--long"; 

  if (SearchMode == APROPOS_REGEX)
  {
    pArguments [n++] = "--regex";
  }

  if ((SearchMode == APROPOS_WILDCARD)
        || (SearchMode == APROPOS_WILDCARD_EXACT))
  {
    pArguments [n++] = "--wildcard";
  }

  if ((SearchMode == APROPOS_EXACT)
        || (SearchMode == APROPOS_WILDCARD_EXACT))
  {
    pArguments [n++] = "--exact";
  }

  pArguments [n++] = pSearchKeyword; 
  pArguments [n] = NULL;


  /*  Run apropos(1) as a child process.  Return false if an error occurs.
  */

  if (!CreateChildProcess (&pid, pErrorOut, pExecutable, pArguments,
                           STDIN_NULL | STDOUT_REDIRECT | STDERR_NULL, 
                           NULL, &fdOutput, NULL))
    return false;


  /*  Capture the output from apropos(1) into a buffer.
  */

  CaptureInput (fdOutput, (void**) &pRawData, &cbRawData, 0, ' ');
  close (fdOutput);


  /*  Wait for apropos(1) to terminate; obtain its exit status.
  */

  waitpid (pid, &status, 0);


  /*  If apropos crashed or returned an exit status other than
  *   zero, return false.
  */

  if (WIFSIGNALED (status) || (WEXITSTATUS (status) != 0))
  {
    free (pRawData);

    pErrorOut->context    = ERRORCTXT_RUNTIME;
    pErrorOut->ErrorCode  = status;
    pErrorOut->pExecPath  = pExecutable;

    return false;
  }


  /*  Count the lines in the apropos output.
  */

  nLines = 1;
  for (i = 0; i < cbRawData; i++)
  {
    if (pRawData [i] == '\n')
    {
      nLines++;
    }
  }


  /*  Allocate a memory block large enough to hold the 
  *   APROPOSRESULT array plus the text (consisting of the
  *   page titles, sections, and descriptions).  Initialize
  *   the block to all zeros.
  */

  cbTotal = cbRawData + (sizeof (APROPOSRESULT) + 4) * nLines + 256; 
  pResults = (APROPOSRESULT*) malloc (cbTotal);
  memset (pResults, 0, cbTotal);

  pStr = (char*) (pResults + nLines + 1);


  /*  Apply a regex to extract the page titles, sections, and
  *   descriptions from each line of the raw data.
  */

  i = 0;
  pRawDataEnd = pRawData + cbRawData;
  pNextLine = pRawData;  

  while ((pLine = pNextLine) != NULL)
  {
    cbRemaining = pRawDataEnd - pLine;
    pNextLine = (char*) memchr (pLine, '\n', cbRemaining);
    length = (pNextLine == NULL)
                ? cbRemaining : (pNextLine++ - pLine);

  
    if (tre_regnexec (&AproposRegex, pLine, length, 4, match, 0))
      continue;


    n = match [1].rm_eo - match [1].rm_so;
    memcpy (pStr, pLine + match [1].rm_so, n);
    pPageTitle = pStr;
    pStr += n + 1;
  
    n = match [2].rm_eo - match [2].rm_so;
    memcpy (pStr, pLine + match [2].rm_so, n);
    pSection = pStr;
    pStr += n + 1;
  
    n = match [3].rm_eo - match [3].rm_so;
    memcpy (pStr, pLine + match [3].rm_so, n);
    pDescription = pStr;
    pStr += n + 1;

    pResults [i].pPageTitle    = pPageTitle;
    pResults [i].pSection      = pSection;
    pResults [i].pDescription  = pDescription;
    i++;
  }


  /*  Free the raw data--we no longer need it.
  */

  free (pRawData);


  /*  If no valid results were found, return false.
  */

  if (i == 0)
  {
    free (pResults);

    pErrorOut->context    = ERRORCTXT_RUNTIME;
    pErrorOut->ErrorCode  = 16;
    pErrorOut->pExecPath  = pExecutable;

    return false;
  } 


  /*  Shrink the buffer to free unused space at the end.
  */

  cb = pStr - ((char*) pResults) + 1;
  if (cbTotal - cb >= 64)
  {
    pResults = (APROPOSRESULT*) realloc (pResults, cb);
  }  


  *ppResultsOut = pResults;
  *pnResultsOut = i;

  return true;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                      InfoFileFromKeyword
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

int InfoFileFromKeyword
   (const char         *pKeyword,
    char              **ppFileOut,
    PROCESSERRORINFO   *pErrorOut)

{
  int i, t, fdOutput, length, status = 0;
  pid_t pid;
  char c, *pFilename, *pBasename;
  const char *pCommand, *pExtension;

  const char *pExecutable = InfoPath;

  static const char *extensions []
          = {".z", ".gz", ".xz", ".bz2", ".lz", ".lzma", ".Z", ".Y", NULL};


  *ppFileOut = NULL;


  /*  Run info(1) as a child process.  Return INFO_ERROR if an error occurs.
  */

  pCommand = strrchr (pExecutable, '/');
  pCommand = (pCommand == NULL) ? pExecutable : (pCommand + 1);

  const char *pArguments [] 
     = { pCommand, "-w", pKeyword, NULL };


  if (!CreateChildProcess (&pid, pErrorOut, pExecutable, pArguments,
                           STDIN_NULL | STDOUT_REDIRECT | STDERR_NULL, 
                           NULL, &fdOutput, NULL))
    return INFO_ERROR;


  /*  Capture the output from info(1) into a buffer.
  */

  CaptureInput (fdOutput, (void**) &pFilename, &length, PATH_MAX, '\0');
  close (fdOutput);


  /*  Wait until the process completes.  Return INFO_ERROR if it appears that
  *   the process crashed.
  */

  waitpid (pid, &status, 0);


  if (WIFSIGNALED (status))
  {
    free (pFilename);

    pErrorOut->context    = ERRORCTXT_RUNTIME;
    pErrorOut->ErrorCode  = status;
    pErrorOut->pExecPath  = pExecutable;

    return INFO_ERROR;
  }


  /*  Remove trailing whitespace.
  */

  while ((length > 0)
           && (c = pFilename [length - 1], 
               ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'))))
  {
    length--;
  }


  if (length <= 0)
  {
    free (pFilename);
    return INFO_NOT_FOUND;
  }


  pFilename [length] = '\0';

  if (strcasecmp (pFilename, "*manpages*") == 0)
  {
    free (pFilename);
    return INFO_REDIRECT_TO_MAN_PAGE;
  }


  /*  Skip past directory names.  (info usually, but not always, returns
  *   a fully-qualified path.)
  */

  if ((pBasename = (char*) strrchr (pFilename, '/')) == NULL)
  {
    pBasename = pFilename;
  }
  else
  {
    pBasename++;
    length -= pBasename - pFilename;
  }

  
  /*  Remove the file extension.
  */

  i = 0;
  while (((pExtension = extensions [i]) != NULL)
            && (((t = strlen (pExtension)) >= length)
                 || (strcmp (pBasename + (length - t), pExtension) != 0)))
  {
    i++;
  }

  if (pExtension != NULL)
  {
    length -= t;
    pBasename [length] = '\0';
  }

  if ((length > 5) && (strcasecmp (pBasename + length - 5, ".info") == 0))
  {
    pBasename [length - 5] = '\0';
  }


  *ppFileOut = strdup (pBasename);

  free (pFilename);
  return INFO_SUCCESS;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                           GetInfoContent
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

bool GetInfoContent
   (const char         *pInfoFile,
    const char         *pNodeName,
    char              **ppDataOut,
    int                *pcbDataOut,
    PROCESSERRORINFO   *pErrorOut)

{
  int fdOutput, cbData, status = 0;
  pid_t pid;
  char *pData;
  const char *pCommand;
  const char *pExecutable = InfoPath;


  *ppDataOut = NULL;
  *pcbDataOut = 0;


  /*  Construct the argument list.
  */

  pCommand = strrchr (pExecutable, '/');
  pCommand = (pCommand == NULL) ? pExecutable : (pCommand + 1);

  const char *pArguments [] =
       { pCommand, "-o", "-", pInfoFile, "-n", pNodeName, NULL };


  /*  Run "info" as a child process.
  */

  if (!CreateChildProcess (&pid, pErrorOut, pExecutable, pArguments,
                           STDIN_NULL | STDOUT_REDIRECT | STDERR_NULL, 
                           NULL, &fdOutput, NULL))
    return false;


  /*  Capture the output from info(1) into a buffer.
  */

  CaptureInput (fdOutput, (void**) &pData, &cbData, 0, ' ');
  close (fdOutput);


  /*  Wait for the process to terminate.
  */

  waitpid (pid, &status, 0);


  /*  If the info(1) process terminated as the result of a signal,
  *   fill out the PROCESSERRORINFO structure and return false.
  *   We don't check the exit code, since info (apparently) always
  *   returns 0, even when an invalid file or node name was
  *   specified.
  */

  if (WIFSIGNALED (status))
  {
    free (pData);

    pErrorOut->context    = ERRORCTXT_RUNTIME;
    pErrorOut->ErrorCode  = status;
    pErrorOut->pExecPath  = pExecutable;

    return false;    
  }


  /*  Handle the case where info(1) produced no output, indicating that
  *   the specified node wasn't found.  Return true, since technically
  *   no error occurred.
  */

  if (cbData == 0)
  {
    free (pData);
    return true;
  }


  *ppDataOut = pData;
  *pcbDataOut = cbData;

  return true;
}


