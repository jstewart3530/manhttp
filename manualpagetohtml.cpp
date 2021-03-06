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

#include "utility.h"                   /*  Application headers.  */
#include "html_formatting.h"
#include "manualpagetohtml.h"
#include "common_js.h"



/*  Dynamically-generated headers.
*/

#include "dynamic/man_page_script.h"



#define HTML_EXPANSION_FACTOR     10


 
enum LINECLASSIFICATION
{
  LINE_CLASS_NONE,
  LINE_CLASS_BLANK,
  LINE_CLASS_TEXT,
  LINE_CLASS_SECTION_TITLE,

  /*  The following are for future use.  */
  LINE_CLASS_HEADER,
  LINE_CLASS_FOOTER,
  LINE_CLASS_TABLE_TOP,
  LINE_CLASS_TABLE_BOTTOM,
  LINE_CLASS_TABLE_ROW,
  LINE_CLASS_TABLE_DIVIDER
};


struct SECTIONENTRY
{
  SECTIONENTRY  *pNext;
  char          *pTitle;
};



/*  Function prototypes.
*/

static LINECLASSIFICATION ClassifyLine (const char*, const TEXTATTRIBUTES*, int, LINECLASSIFICATION);
static void GetTextAttributes (const char*, int, char*, TEXTATTRIBUTES*, int, int*);
static void AnsiGetTextAttributes (const char*, int, char*, TEXTATTRIBUTES*, int, int*);
static void OldStyleGetTextAttributes (const char*, int, char*, TEXTATTRIBUTES*, int, int*);
static void HandleSplitLinks (const char*, int, TEXTATTRIBUTES*);



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                         ManualPageToHTML
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

void ManualPageToHTML
    (FILE          *stream,
     const char    *pPageTitle,
     const char    *pUriPrefix,
     const char    *pStylesheet,
     const char    *pContent,
     int            cbContent)

{
  int i, j, length, iLine, iNextLine, cbText, iSectionStart, iSectionEnd;
  int nSections, indent, MinIndent;
  char c, *pText;
  bool fLastLine;
  LINECLASSIFICATION LineClass;
  TEXTATTRIBUTES *pAttributes;
  SECTIONENTRY *pFirstSection, *pLastSection, *pSection, *pNextSection;
  char buffer [128];


  const HTMLFORMATINFO FormatInfo
           = { 
               .pWebLinkAttrs      = "RefType=\"uri\"", 
               .pManPageLinkAttrs  = "RefType=\"manpage\"", 
               .pInfoLinkAttrs     = "", 
               .pInfoContextName   = "",
               .pUriPrefix         = pUriPrefix
             };


  HTMLizeText (buffer, sizeof (buffer), pPageTitle, -1, NULL, NULL, 0);

  fprintf (stream, 
           "<!DOCTYPE html>\n\n"
           "<html>\n"
           "<head>\n"
           "<title>Man page: %s</title>\n"
           "<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n"
           "<base href=\"%s\">\n"
           "<style>\n%s\n</style>\n"
           "</head>\n"
           "<body Type=\"man\">\n"
           "<div id=\"Main\">\n"
           "<div class=\"PageTitle\">%s</div>\n\n\n"
           "<div id=\"Prologue\">\n",
           buffer, 
           pUriPrefix,
           pStylesheet, 
           buffer);


  pText = (char*) malloc (cbContent + 1);         
  pAttributes = (TEXTATTRIBUTES*) malloc (sizeof (TEXTATTRIBUTES) * (cbContent + 1));

  GetTextAttributes (pContent, cbContent, 
                     pText, pAttributes, cbContent + 1, 
                     &cbText);


  nSections = 0;
  iSectionStart = iSectionEnd = -1;
  LineClass = LINE_CLASS_NONE;
  pFirstSection = pLastSection = NULL;
  fLastLine = false;
  
  for (iLine = 0; !fLastLine; iLine = iNextLine)
  {
    j = -1;
    indent = 0;
    for (i = iLine; (i < cbText) && ((c = pText [i]) != '\n'); i++)
    {
      if ((c == ' ') || (c == '\t'))
      {
        if (j < 0)
        {
          indent++;
        }
      }  
      else
      {
        j = i;
      }
    }

    length = (j >= 0) ? (j - iLine + 1) : 0;
    iNextLine = i + 1;
    fLastLine = (iNextLine >= cbText);


    LineClass = ClassifyLine (pText + iLine, pAttributes + iLine, length, LineClass);


    if ((LineClass == LINE_CLASS_BLANK) && !fLastLine)
      continue;


    if (LineClass == LINE_CLASS_TEXT)
    {
      if (iSectionStart < 0)
      {
        iSectionStart = iLine;
        MinIndent = indent;
      }

      if (indent < MinIndent)
      {
        MinIndent = indent;
      }

      iSectionEnd = iLine + length;

      if (!fLastLine)
        continue;
    }


    if (iSectionStart >= 0)
    {
      int cbSection = iSectionEnd - iSectionStart;
      int cbMax = cbSection * HTML_EXPANSION_FACTOR;
      char *pMarkup = (char*) malloc (cbMax);
      char *pSectionText = pText + iSectionStart;
      TEXTATTRIBUTES *pSectionAttrs = pAttributes + iSectionStart;


      NormalizeAttributesOfWhitespace (pSectionText, pSectionAttrs, cbSection, TEXT_ATTR_BOLD);
      NormalizeAttributesOfWhitespace (pSectionText, pSectionAttrs, cbSection, TEXT_ATTR_ITALIC);

      RecognizeURIs (pSectionText, cbSection, pSectionAttrs);
      RecognizeManPageRefs (pSectionText, cbSection, pSectionAttrs);
      HandleSplitLinks (pSectionText, cbSection, pSectionAttrs);

      HTMLizeText (pMarkup, cbMax, pSectionText, cbSection,
                   pSectionAttrs, &FormatInfo, MinIndent);
            
      fprintf (stream, "<pre>\n%s\n</pre>\n", pMarkup);
      free (pMarkup);

      iSectionStart = iSectionEnd = -1;
    }


    if (LineClass == LINE_CLASS_SECTION_TITLE)
    {
      nSections++;
      HTMLizeText (buffer, sizeof (buffer), pText + iLine, length, NULL, NULL, 0);

      pSection = (SECTIONENTRY*) malloc (sizeof (SECTIONENTRY));
      pSection->pNext   = NULL;
      pSection->pTitle  = strdup (buffer);

      if (pLastSection == NULL)
      {
        pFirstSection = pSection;
      }
      else
      {
        pLastSection->pNext = pSection;
      }

      pLastSection = pSection;


      fprintf (stream,   
               "</div>\n\n\n"            
               "<div class=\"HeaderBar\" id=\"Sec%d_Header\">\n"
               "%s\n"                             
               "<span id=\"Sec%d_ShowBtn\" class=\"HideButton\" onclick=\"ShowSection (%d, 'toggle')\"></span>\n"
               "</div>\n\n"               
               "<div class=\"Collapsible\" id=\"Sec%d\">\n",
               nSections,
               buffer,                  
               nSections, nSections, nSections);
    }
  }


  free (pAttributes);
  free (pText);


  fprintf (stream,
           "</div>\n"
           "</div>\n");


  fprintf (stream, 
           "\n\n<div id=\"NavBar\">\n"
           "<div id=\"Nav-Man-GoToSection\">\n"
           "<span Label=\"1\">Go to section:</span>\n"
           "<select id=\"SectionList\">\n"
           "   <option value=\"x\">-----</option>\n");


  char SectionTitle [64], SectionTitle2 [64];

  for (pSection = pFirstSection, i = 1
        ; pSection != NULL
        ; pSection = pNextSection, i++)
  {
    EllipsizeString (pSection->pTitle, SectionTitle, 
                     sizeof (SectionTitle), 24);

    HTMLizeText (SectionTitle2, sizeof (SectionTitle2), 
                 SectionTitle, -1, NULL, NULL, 0);

    fprintf (stream, 
             "   <option value=\"%d\">%s</option>\n",
             i, SectionTitle2);

    pNextSection = pSection->pNext;
    free (pSection->pTitle);
    free (pSection);
  }


  fprintf (stream, "</select>\n");

  fprintf (stream, 
           "&nbsp;\n"
           "<span id=\"ShowAllBtn\" class=\"Button\">Show all sections</span>\n"
           "&nbsp;\n"
           "<span id=\"HideAllBtn\" class=\"Button\">Hide all sections</span>\n"
           "</div>\n");

  fprintf (stream, "</div>\n\n\n");


  fprintf (stream, 
           "<script>\n"
           "\"use strict\";\n"
           "%s\n"
           "const nSections = %d;\n"
           "%s"
           "</script>\n", 
           AdjustMarginCode,
           nSections,
           ScriptCode);

  fprintf (stream, 
           "</body>\n"
           "</html>\n");
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                             ClassifyLine
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static 
LINECLASSIFICATION ClassifyLine 
    (const char            *pText, 
     const TEXTATTRIBUTES  *pAttributes, 
     int                    length, 
     LINECLASSIFICATION     PreviousLineClass)

{
  int i;


  if (length <= 0)
    return LINE_CLASS_BLANK;


  if ((pText [0] != ' ') && (pText [0] != '\t'))
  {
    i = 0;
    while ((i < length) 
              && ((pAttributes [i] & TEXT_ATTR_APPEARANCE_MASK) == TEXT_ATTR_BOLD))
    {
      i++;
    }
  
    if (i == length)
      return LINE_CLASS_SECTION_TITLE;
  }

  return LINE_CLASS_TEXT;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                        GetTextAttributes
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void GetTextAttributes
   (const char      *pText,
    int              cbText,
    char            *pTextOut,
    TEXTATTRIBUTES  *pAttrsOut,
    int              cbMax,
    int             *pLengthOut)

{
  int i, nEscapes = 0;
  bool fIsANSI;


  for (i = 0; i < cbText; i++)
  {
    if ((pText [i] == 0x1b) && (pText [i + 1] == '['))
    {
      nEscapes++;
    }
  }
  
  fIsANSI = (nEscapes >= 5);


  (fIsANSI ? AnsiGetTextAttributes : OldStyleGetTextAttributes)
      (pText, cbText, pTextOut, pAttrsOut, cbMax, pLengthOut);
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                    AnsiGetTextAttributes
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void AnsiGetTextAttributes
   (const char      *pText,
    int              cbText,
    char            *pTextOut,
    TEXTATTRIBUTES  *pAttrsOut,
    int              cbMax,
    int             *pLengthOut)

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


    pTextOut [j] = (c == '\r') ? ' ' : c;
    pAttrsOut [j] = attrs;
    j++;
  }


  pTextOut [j] = '\0';
  pAttrsOut [j] = 0;


  if (pLengthOut != NULL)
  {
    *pLengthOut = j;
  }
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                OldStyleGetTextAttributes
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void OldStyleGetTextAttributes
   (const char      *pText,
    int              cbText,
    char            *pTextOut,
    TEXTATTRIBUTES  *pAttrsOut,
    int              cbMax,
    int             *pLengthOut)

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

  if (pLengthOut != NULL)
  {
    *pLengthOut = j;
  }
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                         HandleSplitLinks
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void HandleSplitLinks 
   (const char       *pText,
    int               cbText,
    TEXTATTRIBUTES   *pAttrs)

{
  /*  Just a stub for now.  */
}   
