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


#ifndef __UTILITY_H_
#define __UTILITY_H_



/*  Flags used with CreateChildProcess().
*/

enum
{
  STDIN_REDIRECT   = 0x0001,
  STDIN_NULL       = 0x0002,
  STDOUT_REDIRECT  = 0x0004,
  STDOUT_NULL      = 0x0008,
  STDERR_REDIRECT  = 0x0010,
  STDERR_NULL      = 0x0020
};


extern "C"
{
extern bool EllipsizeString
   (const char   *pSourceStr,
    char         *pDestStr,
    int           cbMax,
    int           nMaxChars);


extern bool LoadFile
   (const char   *pPath,
    char        **ppContentOut,
    int          *pcbOut,
    char        **ppErrorOut);

   
extern int NormalizeSpaces 
   (const char  *pStr, 
    char        *pDest,
    int          cbMax);


extern bool JSEscapeString 
   (const char  *pStrSource, 
    char        *pStrDest, 
    int          cbDestMax);
   

extern int CountCharsUTF8
   (const char  *pStr,
    int          cbStr);


extern void SetCloseOnExec
   (int   fd,
    bool  fCloseOnExec);


extern bool CreateChildProcess
   (pid_t          *pidOut,     
    int            *pResultOut,   
    const char     *pszExecutable,
    const char    **ppszArguments,
    int             flags,
    int            *pfdStdInput,
    int            *pfdStdOutput,
    int            *pfdStdError);


extern int CaptureInput
   (int      fd,
    void   **ppDataOut,
    int     *pcbDataOut,
    int      cbMaxData,
    char     cZeroReplace);

}

#endif
