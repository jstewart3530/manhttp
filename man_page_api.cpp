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
#include "man_page_api.h"



static regex_t ParseRegex;



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
                                                     manInitializeRegexes
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

void manInitializeRegexes
   (void)

{
  int error;


  error = tre_regcomp 
             (&ParseRegex, 
              "^[[:space:]]*([a-z0-9.+_:-]+)[[:space:]]*(\\([0-9]{0,3}[a-z]{0,3}\\))?[[:space:]]*$", 
              REG_EXTENDED | REG_ICASE | REG_NEWLINE);

  if (error != 0)
  {
    fprintf (stderr, "FATAL ERROR: regcomp() failed for ParseRegex.  (Error %d)\n", 
             error);
    exit (100);
  }
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                        GetManPageContent
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

bool GetManPageContent
   (const char          *pExecutable,
    const char          *pPageTitle,
    const char          *pSection,
    char               **ppDataOut,
    int                 *pcbDataOut,
    PROCESSERRORINFO    *pErrorOut)

{
  int nArgs = 0, fdOutput, status = 0, cbData;
  pid_t pid;
  char *pData;
  const char *pCommand, *pArguments [8];


  *ppDataOut = NULL;
  *pcbDataOut = 0;

 
  /*  Construct the argument list.
  */

  pArguments [nArgs++] = ((pCommand = strrchr (pExecutable, '/')) == NULL)
                             ? pExecutable
                             : (pCommand + 1);

#if !defined(TARGET_BSD)
  pArguments [nArgs++] = "--encoding=UTF-8";
#endif

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
   (const char          *pExecutable,
    const char          *pSearchKeyword,
    APROPOSRESULT      **ppResultsOut,
    int                 *pnResultsOut,
    PROCESSERRORINFO    *pErrorOut)

{
  int i, status = 0, index, fdOutput, nLines, cbData;
  int nArgs = 0;
  pid_t pid;
  char c, *p, *pData, *pLine, *pNextLine;
  char *pPageTitle, *pSection, *pDescription;
  APROPOSRESULT *pResults;
  const char *pCommand, *pArguments [5];


  *ppResultsOut = NULL;
  *pnResultsOut = 0;

  
  /*  Construct the argument list.
  */

  pArguments [nArgs++] = ((pCommand = strrchr (pExecutable, '/')) == NULL)
                             ? pExecutable
                             : (pCommand + 1);

#if !defined(TARGET_BSD)
  pArguments [nArgs++] = "--long";
#endif

  pArguments [nArgs++] = pSearchKeyword;
  pArguments [nArgs] = NULL;


  /*  Run apropos(1) as a child process.
  */

  if (!CreateChildProcess (&pid, pErrorOut, pExecutable, pArguments,
                           STDIN_NULL | STDOUT_REDIRECT | STDERR_NULL, 
                           NULL, &fdOutput, NULL))
    return false;


  /*  Capture the output from apropos(1) into a buffer.
  */

  CaptureInput (fdOutput, (void**) &pData, &cbData, 0, ' ');
  close (fdOutput);


  /*  Wait for apropos(1) to terminate; obtain its exit status.
  */

  waitpid (pid, &status, 0);


  if (WIFSIGNALED (status) || (WEXITSTATUS (status) != 0))
  {
    free (pData);

    pErrorOut->context    = ERRORCTXT_RUNTIME;
    pErrorOut->ErrorCode  = status;

    return false;
  }


  /*  Count the lines in the apropos output.
  */

  nLines = 1;
  for (i = 0; i < cbData; i++)
  {
    if (pData [i] == '\n')
    {
      nLines++;
    }
  }


  /*  Allocate a buffer large enough to hold one APROPOSRESULT
  *   structure per line plus the output text itself.
  */

  pResults = (APROPOSRESULT*) malloc (sizeof (APROPOSRESULT) * nLines + cbData + 1);

  memset (pResults, 0, sizeof (APROPOSRESULT) * nLines);

  pLine = (char*) (pResults + nLines);
  memcpy (pLine, pData, cbData);
  pLine [cbData] = '\0';
  free (pData);


  index = 0;
  for (; pLine != NULL; pLine = pNextLine)
  {
    if ((pNextLine = strchr (pLine, '\n')) != NULL)
    {
      *pNextLine = '\0';
      pNextLine++;
    }

    
    p = pLine;


    /*  Get the page title.
    */

    while (c = *p, ((c == ' ') || (c == '\t')))
    {
      p++;
    }

    if (c == '\0')
      continue;

    pPageTitle = p;

    while (c = *p, ((c != ' ') && (c != '\t') && (c != '\0')))
    {
      p++;
    }
    
    if (c == '\0')
      continue;
    
    *(p++) = '\0';

 
    /*  Get the section name.
    */

    while (c = *p, ((c != '(') && (c != '\0')))
    {
      p++;
    }

    if (c == '\0')
      continue;

    p++;

    while (c = *p, ((c == ' ') || (c == '\t')))
    {
      p++;
    }
    
    if (c == '\0')
      continue;

    pSection = p;

    while (c = *p, ((c != ')') && (c != '\0')))
    {
      p++;
    }

    if (c == '\0')
      continue;
    
    *(p++) = '\0';


    /*  Get the description.
    */

    while (c = *p, ((c != '-') && (c != '\0')))
    {
      p++;
    }

    if (c == '\0')
      continue;

    p++;

    while (c = *p, ((c == ' ') || (c == '\t')))
    {
      p++;
    }
    
    if (c == '\0')
      continue;

    pDescription = p;    


    pResults [index].pPageTitle    = pPageTitle;
    pResults [index].pSection      = pSection;
    pResults [index].pDescription  = pDescription;
    index++;
  }
 

  *ppResultsOut = pResults;
  *pnResultsOut = index;

  return true;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                      InfoFileFromKeyword
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

int InfoFileFromKeyword
   (const char         *pExecutable,
    const char         *pKeyword,
    char              **ppFileOut,
    PROCESSERRORINFO   *pErrorOut)

{
  int i, t, nArgs = 0, fdOutput, length, status = 0;
  pid_t pid;
  char c, *pFilename, *pBasename;
  const char *pCommand, *pExtension, *pArguments [4];

  static const char *extensions []
          = {".z", ".gz", ".xz", ".bz2", ".lz", ".lzma", ".Z", ".Y", NULL};


  *ppFileOut = NULL;


  /*  Construct the argument list.
  */

  pArguments [nArgs++] = ((pCommand = strrchr (pExecutable, '/')) == NULL)
                             ? pExecutable
                             : (pCommand + 1);

  pArguments [nArgs++] = "-w";
  pArguments [nArgs++] = pKeyword;
  pArguments [nArgs] = NULL;


  /*  Run info(1) as a child process.
  */

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
   (const char         *pExecutable,
    const char         *pInfoFile,
    const char         *pNodeName,
    char              **ppDataOut,
    int                *pcbDataOut,
    PROCESSERRORINFO   *pErrorOut)

{
  int fdOutput, cbData, status = 0;
  pid_t pid;
  char *pData;
  const char *pCommand;


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


