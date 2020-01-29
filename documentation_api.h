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


#ifndef _MAN_PAGE_API_H_
#define _MAN_PAGE_API_H_



/*  Structure returned by GetAproposContent().
*/

struct APROPOSRESULT
{
  char  *pPageTitle;
  char  *pSection;
  char  *pDescription;
};


enum APROPOSMODE
{
  APROPOS_REGEX      = 1,
  APROPOS_WILDCARD,
  APROPOS_EXACT,
  APROPOS_WILDCARD_EXACT
};



enum
{
  INFO_SUCCESS                = 0,
  INFO_ERROR                  = 1,
  INFO_NOT_FOUND              = -1,
  INFO_REDIRECT_TO_MAN_PAGE   = -2
};



extern "C"
{

extern void manInitializeRegexes
   (void);


extern bool ParseManPageTitle
   (const char   *pStr,
    char         *pTitleOut,
    int           cbTitleMax,
    char         *pSectionOut,
    int           cbSectionMax);   


extern bool GetManPageContent
   (const char          *pPageTitle,
    const char          *pSection,
    char               **ppDataOut,
    int                 *pcbDataOut,
    PROCESSERRORINFO    *pErrorOut);


extern bool GetAproposContent
   (const char          *pSearchKeyword,
    APROPOSMODE          SearchMode,
    APROPOSRESULT      **ppResultsOut,
    int                 *pnResultsOut,
    PROCESSERRORINFO    *pErrorOut);


extern int InfoFileFromKeyword
   (const char         *pKeyword,
    char              **ppFileOut,
    PROCESSERRORINFO   *pErrorOut);
  

extern bool GetInfoContent
   (const char         *pInfoFile,
    const char         *pNodeName,
    char              **ppDataOut,
    int                *pcbDataOut,
    PROCESSERRORINFO   *pErrorOut);

}

#endif

