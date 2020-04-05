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

#include <tre/tre.h>                   /*  Library headers.  */

#include "html_formatting.h"           /*  Application headers.  */
#include "infotohtml.h"



static regex_t UriRegex, PageRefRegex;



/*  Function prototypes.
*/

static char* CreateManPageLinkTag (const char*, const TEXTATTRIBUTES*, const HTMLFORMATINFO*);
static char* CreateInfoLinkTag (const char*, const TEXTATTRIBUTES*, const HTMLFORMATINFO*);
static char* CreateUriLinkTag (const char*, const TEXTATTRIBUTES*, const HTMLFORMATINFO*);
static int GetTextAttributesNewStyle (const char*, int, char*, TEXTATTRIBUTES*, int);
static int GetTextAttributesOldStyle (const char*, int, char*, TEXTATTRIBUTES*, int);



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                    htmlInitializeRegexes
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

void htmlInitializeRegexes
   (void)

{
  int error;

  
  /*  See: http://www.regexguru.com/2008/11/detecting-urls-in-a-block-of-text/
  */
   
  error = tre_regcomp 
              (&UriRegex, 
               "\\b(https?|ftp)://[-A-Z0-9+&@#/%?=~_|!:,.;]*[A-Z0-9+&@#/%=~_|]", 
               REG_EXTENDED | REG_ICASE);

  if (error != 0)
  {
    fprintf (stderr, "FATAL ERROR: tre_regcomp() failed for UriRegex.  (Error %d)\n", 
             error);
    exit (100);
  }


  error = tre_regcomp 
              (&PageRefRegex, 
               "[a-z0-9.+_:-]{1,32}\\([0-9]{1,3}[a-z]{0,4}\\)", 
               REG_EXTENDED | REG_ICASE);

  if (error != 0)
  {
    fprintf (stderr, "FATAL ERROR: tre_regcomp() failed for PageRefRegex.  (Error %d)\n", 
             error);
    exit (100);
  }
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                        GetTextAttributes
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

int GetTextAttributes
   (const char      *pText,
    int              cbText,
    char            *pTextOut,
    TEXTATTRIBUTES  *pAttrsOut,
    int              cbMax)

{
  int i, n;
  int (*pfn) (const char*, int, char*, TEXTATTRIBUTES*, int);

  
  for (i = n = 0; i < cbText; i++)
  {
    if (pText [i] == '\b')
    {
      n++;
    }
  }

  
  pfn = (n >= 5)
            ? GetTextAttributesOldStyle
            : GetTextAttributesNewStyle;

  return pfn (pText, cbText, pTextOut, pAttrsOut, cbMax);
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                GetTextAttributesNewStyle
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
int GetTextAttributesNewStyle
   (const char      *pText,
    int              cbText,
    char            *pTextOut,
    TEXTATTRIBUTES  *pAttrsOut,
    int              cbMax)

{
  int i, j, iStart, length, value;
  bool fNumeric;
  TEXTATTRIBUTES attrs;
  char c;
  unsigned char d;


  i = j = 0;
  attrs = 0;
   
  while ((i < cbText) && (j < cbMax - 1))
  {
  	c = pText [i++];


    if ((c == 0x1b) && (pText [i] == '['))
    {
      iStart = ++i;
      value = 0;
      fNumeric = true;

      while ((i < cbText)
      	        && (d = (unsigned char) pText [i], ((d < 0x40) || (d > 0x7e))))
      {
      	if ((d < '0') || (d > '9'))
      	{
      	  fNumeric = false;	
      	}

        if (fNumeric)
      	{
      	  value = value * 10 + (int) (d - 0x30);	
      	}

      	i++;      	
      }

      length = ++i - iStart;

      if ((d == 'm') && fNumeric && (length >= 2))
      {
        switch (value)
        {
          case 0:   attrs = 0;                   break;
          case 1:   attrs |= TEXT_ATTR_BOLD;     break;
          case 4:   attrs |= TEXT_ATTR_ITALIC;   break;
          case 22:  attrs &= ~TEXT_ATTR_BOLD;    break;
          case 24:  attrs &= ~TEXT_ATTR_ITALIC;
        }
      }

      continue;
    }


    if (c == '\r')
    {
      c = ' ';
    }

    pTextOut [j] = c;
    pAttrsOut [j] = attrs;
    j++;
  }


  pTextOut [j] = '\0';
  pAttrsOut [j] = 0;

  return j;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                GetTextAttributesOldStyle
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
int GetTextAttributesOldStyle
   (const char      *pText,
    int              cbText,
    char            *pTextOut,
    TEXTATTRIBUTES  *pAttrsOut,
    int              cbMax)

{
  int i, j;
  char c, c2;
  TEXTATTRIBUTES attr;


  i = j = 0;
   
  while ((i < cbText) && (j < cbMax - 1))
  {
    attr = 0;
    c = pText [i++];

    if ((c == '_') 
          && (i < cbText - 2) 
          && (pText [i] == '\b') 
          && ((c2 = pText [i + 1]) != '_'))
    {
      i += 2;
      attr |= TEXT_ATTR_ITALIC;
      c = c2;
    }

    if ((i < cbText - 2)
           && (pText [i] == '\b')
           && (pText [i + 1] == c))
    {
      i += 2;
      attr |= TEXT_ATTR_BOLD;
    }

    pTextOut [j] = (c == '\r') ? ' ' : c;
    pAttrsOut [j] = attr;
    j++;
  }


  pTextOut [j] = '\0';
  pAttrsOut [j] = 0;

  return j;  
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                          NormalizeAttributesOfWhitespace
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

void NormalizeAttributesOfWhitespace 
   (const char      *pText, 
    TEXTATTRIBUTES  *pAttributes,
    int              length,    
    TEXTATTRIBUTES   attr)

{
  int i, j, iFirst;
  char c;


  iFirst = -1;
  for (i = 0;; i++)
  {
    c = ((length >= 0) && (i >= length)) ? '\0' : pText [i];

    if ((c == '\0') || ((pAttributes [i] & attr) != attr))
    {
      if (iFirst >= 0)
      {
        for (j = iFirst + 1; j < i; j++)
        {
          pAttributes [j] &= ~attr;
        }

        iFirst = -1;
      }

      if (c == '\0')
        break;
    }
    else if ((c != ' ') && (c != '\t'))
    {
      iFirst = i;
    }
  }
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                              HTMLizeText
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

bool HTMLizeText 
   (char                  *pDest,
    int                    cbMax,
  	const char            *pText, 
    int                    cbText, 
    const TEXTATTRIBUTES  *pAttributes,
    const HTMLFORMATINFO  *pFmtInfo,
    int                    nLeadingSpacesToTrim)

{
  int index, cbOutput, cbEscape, length;
  int IndentState, nSpaces;
  char c, *pTag;
  const char *pEscape;
  bool fInLink = false;
  TEXTATTRIBUTES attrs, CurrentAttrs, StartAttrs, EndAttrs, mask;


  CurrentAttrs = 0;
  cbOutput = 0;
  IndentState = nSpaces = 0;

  mask = (pFmtInfo == NULL)
            ? TEXT_ATTR_APPEARANCE_MASK
            : (TEXT_ATTR_APPEARANCE_MASK | TEXT_ATTR_LINK_MASK);


  for (index = 0;; index++)
  {
    c = ((cbText < 0) || (index < cbText)) ? pText [index] : '\0';
    if (c == '\r')
      continue;


    if (nLeadingSpacesToTrim > 0)
    {
      switch (c)
      {
        case ' ':
        case '\t':
          if (IndentState == 0)
          {
            if (++nSpaces <= nLeadingSpacesToTrim)           
              continue;
            IndentState = 1;
          }
          break;

        case '\n':
          IndentState = nSpaces = 0;
          break;

        default:
          IndentState = 2;
      }
    }

    
    attrs = ((c == '\0') || (pAttributes == NULL)) 
                 ? 0 : (pAttributes [index] & mask);

    StartAttrs = attrs & ~CurrentAttrs;
    EndAttrs = ~attrs & CurrentAttrs;


    /*  Generate end tags as necessary.
    */

    if (fInLink && (EndAttrs & TEXT_ATTR_LINK_MASK))
    {
      if (cbMax - cbOutput < 5)
        break;
      memcpy (pDest + cbOutput, "</a>", 4);
      cbOutput += 4;

      fInLink = false;
      CurrentAttrs &= ~TEXT_ATTR_LINK_MASK;
    }

    if (EndAttrs & TEXT_ATTR_ITALIC)
    {
      if (cbMax - cbOutput < 8)
        break;
      memcpy (pDest + cbOutput, "</span>", 7);
      cbOutput += 7;

      CurrentAttrs &= ~TEXT_ATTR_ITALIC;
    }

    if (EndAttrs & TEXT_ATTR_BOLD)
    {
      if (cbMax - cbOutput < 8)
        break;
      memcpy (pDest + cbOutput, "</span>", 7);
      cbOutput += 7;
      
      CurrentAttrs &= ~TEXT_ATTR_BOLD;
    }


    /*  Exit the loop if we've reached the end of the text.
    */

    if (c == '\0')
      break;


    /*  Generate start tags as necessary.  Note that the attributes appear in the 
    *   order opposite of the order above.
    */

    if (StartAttrs & TEXT_ATTR_BOLD)
    {
      if (cbMax - cbOutput < 14)
        break;
      memcpy (pDest + cbOutput, "<span Bold=\"\">", 14);
      cbOutput += 14;
    }


    if (StartAttrs & TEXT_ATTR_ITALIC)
    {
      if (cbMax - cbOutput < 14)
        break;
      memcpy (pDest + cbOutput, "<span Ital=\"\">", 14);
      cbOutput += 14;
    }


    if ((StartAttrs & TEXT_ATTR_URI)
           && ((pTag = CreateUriLinkTag (pText + index, pAttributes + index, pFmtInfo)) != NULL))
    {
      length = strlen (pTag);
      if (cbMax - cbOutput < length + 1)
      {
        free (pTag);
        break;
      }
                 
      memcpy (pDest + cbOutput, pTag, length);
      free (pTag);

      cbOutput += length;
      fInLink = true;
    }


    if ((StartAttrs & TEXT_ATTR_MAN_PAGE_REF)
           && ((pTag = CreateManPageLinkTag (pText + index, pAttributes + index, pFmtInfo)) != NULL))
    {
      length = strlen (pTag);
      if (cbMax - cbOutput < length + 1)
      {
        free (pTag);
        break;
      }
                 
      memcpy (pDest + cbOutput, pTag, length);
      free (pTag);

      cbOutput += length;
      fInLink = true;
    }


    if ((StartAttrs & TEXT_ATTR_INFO_LINK)
           && ((pTag = CreateInfoLinkTag (pText + index, pAttributes + index, pFmtInfo)) != NULL))
    {
      length = strlen (pTag);
      if (cbMax - cbOutput < length + 1)
      {
        free (pTag);
        break;
      }
                 
      memcpy (pDest + cbOutput, pTag, length);
      free (pTag);

      cbOutput += length;
      fInLink = true;
    }

    CurrentAttrs |= StartAttrs;


    /*  Add the character to the output, substituting an HTML entity if
    *   the character is "&", "<", or ">".
    */  

    switch (c)
    {
      case '&':  pEscape = "&amp;";  cbEscape = 5;  break;
      case '<':  pEscape = "&lt;";   cbEscape = 4;  break;
      case '>':  pEscape = "&gt;";   cbEscape = 4;  break;
      default:   pEscape = NULL;     cbEscape = 1;
    }

    if (cbMax - cbOutput < cbEscape + 1)
      break;
 
    if (pEscape == NULL)
    {
      pDest [cbOutput] = c;
    }
    else
    {
      memcpy (pDest + cbOutput, pEscape, cbEscape);
    }

    cbOutput += cbEscape;
  }


  pDest [cbOutput] = '\0';

  return (c != '\0') || (CurrentAttrs != 0);
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                     CreateManPageLinkTag
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
char* CreateManPageLinkTag
   (const char            *pText,
    const TEXTATTRIBUTES  *pAttributes,
    const HTMLFORMATINFO  *pFmtInfo)

{
  int k;
  bool fNeedAbsoluteURL = false;
  char c, *pTag = NULL;


  for (k = 0
  	     ; ((c = pText [k]) != '\0') 
               && (pAttributes [k] & TEXT_ATTR_MAN_PAGE_REF)
  	     ; k++)
  {
    if (c == ':')
    {
      fNeedAbsoluteURL = true;
    }
  }


  asprintf (&pTag, "<a %s href=\"%sman/%.*s\">", 
            pFmtInfo->pManPageLinkAttrs,
            fNeedAbsoluteURL ? pFmtInfo->pUriPrefix : "",
            k, pText);

  return pTag;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                        CreateInfoLinkTag
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
char* CreateInfoLinkTag
   (const char            *pText,
    const TEXTATTRIBUTES  *pAttributes,
    const HTMLFORMATINFO  *pFmtInfo)

{
  int k, iTargetFirst = -1, iTargetLast = -1, cbTarget;
  int iFileFirst = -1, iFileLast = -1, cbContext;
  char *pEncodedName, *pTag = NULL;
  const char *pTarget, *pContext;


  for (k = 0
         ; (pText [k] != '\0') 
                && (pAttributes [k] & TEXT_ATTR_INFO_LINK)
         ; k++)
  {
    switch (pAttributes [k] & TEXT_ATTR_INFO_BITS_MASK) 
    {
      case TEXT_ATTR_INFO_TARGET:  
        if (iTargetFirst < 0)
        {
          iTargetFirst = k;
        }  
        iTargetLast = k;
        break;

      case TEXT_ATTR_INFO_FILENAME:  
        if (iFileFirst < 0)
        {
          iFileFirst = k;
        }  
        iFileLast = k;
    }  
  }


  if (iTargetFirst >= 0)
  {
    pTarget = pText + iTargetFirst;
    cbTarget = iTargetLast - iTargetFirst + 1;
  }
  else if (iFileFirst >= 0)
  {
    pTarget = "Top";
    cbTarget = 3;
  }
  else
  {
    pTarget = pText;
    cbTarget = k;
  }


  if (iFileFirst >= 0)
  {
    pContext = pText + iFileFirst;
    cbContext = iFileLast - iFileFirst + 1;
  }
  else
  {
    pContext = pFmtInfo->pInfoContextName;
    cbContext = strlen (pFmtInfo->pInfoContextName);
  }


  pEncodedName = EncodeInfoNodeName (pTarget, cbTarget);
  if (pEncodedName == NULL)
  	return NULL;

  asprintf (&pTag, "<a %s href=\"%sinfo/%.*s/%s\">", 
            pFmtInfo->pInfoLinkAttrs,
            (strchr (pEncodedName, ':') != NULL)
                 ? pFmtInfo->pUriPrefix : "",
            cbContext, pContext,
            pEncodedName);

  free (pEncodedName);

  return pTag;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                         CreateUriLinkTag
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
char* CreateUriLinkTag
   (const char            *pText,
    const TEXTATTRIBUTES  *pAttributes,
    const HTMLFORMATINFO  *pFmtInfo)

{
  int k;
  char *pTag = NULL;


  for (k = 0
         ; (pText [k] != '\0') && (pAttributes [k] & TEXT_ATTR_URI)
         ; k++)    
  { /* Empty loop. */ }


  asprintf (&pTag, "<a %s href=\"%.*s\">", 
            pFmtInfo->pWebLinkAttrs,
            k, pText);

  return pTag;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                            RecognizeURIs
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

int RecognizeURIs 
   (const char      *pText, 
    int              length,
    TEXTATTRIBUTES  *pAttrs)

{
  int i, index, iStart, iEnd, count;
  regmatch_t match;


  if (length < 0)
  {
    length = strlen (pText);
  }


  index = count = 0; 
  while ((index < length)
            && !tre_regnexec (&UriRegex, pText + index, length - index, 1, &match, 0))
  {
    iStart = index + match.rm_so;
    iEnd = index + match.rm_eo;

    for (i = iStart; i < iEnd; i++)
    {
      pAttrs [i] = (pAttrs [i] & ~TEXT_ATTR_APPEARANCE_MASK)
                      | TEXT_ATTR_URI;
    }

    count++;
    index = iEnd;
  }

  return count;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                     RecognizeManPageRefs
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

int RecognizeManPageRefs 
   (const char      *pText, 
    int              length,
    TEXTATTRIBUTES  *pAttrs)

{
  int i, index, iStart, iEnd, count;
  bool fValidLink;
  regmatch_t match;

  
  if (length < 0)
  {
    length = strlen (pText);
  }


  index = count = 0; 
  while ((index < length)
            && !tre_regnexec (&PageRefRegex, pText + index, length - index, 1, &match, 0))
  {
    iStart = index + match.rm_so;
    iEnd = index + match.rm_eo;


    fValidLink = true;
    for (i = iStart; i < iEnd; i++)
    {
      if (pAttrs [i] & (TEXT_ATTR_URI | TEXT_ATTR_INFO_LINK))
      {
        fValidLink = false;
        break;
      }
    }

    
    if (fValidLink)
    {
      for (i = iStart; i < iEnd; i++)
      {
        pAttrs [i] = (pAttrs [i] & ~TEXT_ATTR_APPEARANCE_MASK)
                        | TEXT_ATTR_MAN_PAGE_REF;
      }
      count++;
    }

    index = iEnd;
  }

  return count;
}
