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


#ifndef __HTML_FORMATTING_H_
#define __HTML_FORMATTING_H_


typedef unsigned char TEXTATTRIBUTES;



/*  Attribute bits.
*/

enum
{
  /*  Text appearance bits.
  */

  TEXT_ATTR_ITALIC               = 1,
  TEXT_ATTR_BOLD                 = 2,

  TEXT_ATTR_APPEARANCE_MASK      = TEXT_ATTR_BOLD | TEXT_ATTR_ITALIC,


  /*  Hyperlink bits.
  */

  TEXT_ATTR_URI                  = 4,
  TEXT_ATTR_MAN_PAGE_REF         = 8,
  TEXT_ATTR_INFO_LINK            = 16,

  TEXT_ATTR_LINK_SKIP            = 32,

  TEXT_ATTR_LINK_MASK            = TEXT_ATTR_URI
                                      | TEXT_ATTR_MAN_PAGE_REF
                                      | TEXT_ATTR_INFO_LINK,


  /*  Bits to mark the start and end of an info link target.  
  *   Used only by CreateInfoLinkTag().   
  */

  TEXT_ATTR_INFO_TARGET          = 64,  
  TEXT_ATTR_INFO_FILENAME        = 128,

  TEXT_ATTR_INFO_BITS_MASK       = TEXT_ATTR_INFO_TARGET
                                      | TEXT_ATTR_INFO_FILENAME  
};



struct HTMLFORMATINFO
{
  const char  *pWebLinkAttrs;
  const char  *pManPageLinkAttrs;
  const char  *pInfoLinkAttrs;
  const char  *pInfoContextName;
  const char  *pUriPrefix;
};



extern "C"
{

extern void htmlInitializeRegexes
   (void);


extern bool HTMLizeText 
   (char                  *pDest,
    int                    cbMax,
  	const char            *pText,
    int                    cbText, 
    const TEXTATTRIBUTES  *pAttributes,
    const HTMLFORMATINFO  *pFmtInfo,
    int                    nLeadingSpacesToTrim);


extern void NormalizeAttributesOfWhitespace 
   (const char      *pText, 
    TEXTATTRIBUTES  *pAttributes,
    int              length,
    TEXTATTRIBUTES   attr);


extern int RecognizeURIs 
   (const char      *pText, 
    int              length,
    TEXTATTRIBUTES  *pAttrs);


extern int RecognizeManPageRefs 
   (const char      *pText, 
    int              length,
    TEXTATTRIBUTES  *pAttrs);

}

#endif
