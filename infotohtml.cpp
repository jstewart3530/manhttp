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
#include "infotohtml.h"



#define HTML_EXPANSION_FACTOR     10


#define HEX_ESCAPE_CHAR           '~'



enum INFOLINETYPE
{
  INFO_LINE_IGNORABLE,
  INFO_LINE_BLANK,
  INFO_LINE_TEXT,
  INFO_LINE_HEADER,
  INFO_LINE_TITLE,
  INFO_LINE_FOOTNOTES
};


enum INFONODETYPE
{
  INFO_NODE_NORMAL,
  INFO_NODE_INDEX,
  INFO_NODE_DIRECTORY
};


struct INFOLINE
{
  INFOLINETYPE  type;
  char          UnderlineChar;
  int           StartOffset;
  int           EndOffset;
  int           EffectiveLength;
};



struct NAVIGATIONLINKS
{
  char  *pPreviousNode;
  char  *pPreviousNodeUri;
  char  *pNextNode;
  char  *pNextNodeUri;
  char  *pUpNode;
  char  *pUpNodeUri;
};



static regex_t LinkRegex1, LinkRegex2, TitleRegex, DirLinkRegex;



const char ScriptCode []
        = "const MainDiv = document.getElementById (\"Main\");\n"
          "const Navbar = document.querySelector (\".NavBar\");\n"
          "let cxWindow = -1;\n"
          "\n\n"
          "function AdjustTopMargin\n"
          "   (event)\n"
          "\n"
          "{\n"
          "  let cx = window.innerWidth;\n"
          "  if (cx != cxWindow)\n"
          "  {\n"
          "    let cyNavbar = Navbar.offsetHeight;\n"
          "    MainDiv.style.marginTop = `${cyNavbar + 50}px`;\n"
          "    cxWindow = cx;\n"
          "  }\n"
          "}\n"
          "\n\n"
          "AdjustTopMargin (null);\n"
          "window.addEventListener (\"resize\", AdjustTopMargin, false);\n\n";



/*  Function prototypes.
*/

static INFONODETYPE NodeTypeFromName (const char*);
static void GenerateNavBar (FILE*, const NAVIGATIONLINKS*, const HTMLFORMATINFO*);
static void GenerateScript (FILE*, const NAVIGATIONLINKS*, const HTMLFORMATINFO*);
static void GenerateTitle (FILE*, const char*, int, char, bool);
static void ClassifyLines (const char*, int, INFOLINE**, int*);
static void ParseHeaderLine (const char*, int, const char*, const char*, NAVIGATIONLINKS*);
static void RecognizeLinks (INFONODETYPE, const char*, int, TEXTATTRIBUTES*);
static int RecognizeInfoLinks (const char*, int, TEXTATTRIBUTES*);
static int RecognizeInfoIndexLinks (const char*, int, TEXTATTRIBUTES*);
static int RecognizeDirLinks (const char*, int, TEXTATTRIBUTES*);
static bool IsFootnotesLine (const char*, int);
static bool IsUnderline (const char*, int);
static int ValueFromHexDigit (char);



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                    infoInitializeRegexes
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

void infoInitializeRegexes
    (void)

{
  int error;


  error = tre_regcomp 
              (&LinkRegex1, 
               "\\*[[:space:]]*(?:note[[:space:]]+)?([[:alnum:][:space:]+_./?()-]{1,64}?)::", 
               REG_EXTENDED | REG_ICASE);

  if (error != 0)
  {
    fprintf (stderr, "\ntre_regcomp() failed for LinkRegex1 (result code %d).\n\n",
    	       error);
    exit (100);
  }


  error = tre_regcomp 
              (&LinkRegex2, 
               "\\*[[:space:]]*([^:]+:[[:space:]]*([[:alnum:][:space:]+_./?()-]{1,64}))\\.", 
               REG_EXTENDED | REG_ICASE | REG_NEWLINE);

  if (error != 0)
  {
    fprintf (stderr, "\ntre_regcomp() failed for LinkRegex2 (result code %d).\n\n",
    	       error);
    exit (100);
  }  


  error = tre_regcomp 
              (&TitleRegex, 
               "^((?:Appendix[[:space:]]+)?[A-Z0-9][0-9]*(?:\\.[0-9]+)*)[[:space:]]+", 
               REG_EXTENDED | REG_ICASE | REG_NEWLINE);

  if (error != 0)
  {
    fprintf (stderr, "\ntre_regcomp() failed for TitleRegex (result code %d).\n\n",
             error);
    exit (100);
  }    


  error = tre_regcomp 
              (&DirLinkRegex, 
               "^[[:space:]]*\\*[[:space:]]+([[:alnum:][:space:]._+-]+):[[:space:]]+\\(([[:alnum:]._-]+)\\)([[:alnum:][:space:]+_/?()-]*)\\.", 
               REG_EXTENDED | REG_ICASE | REG_NEWLINE);

  if (error != 0)
  {
    fprintf (stderr, "\ntre_regcomp() failed for DirLinkRegex (result code %d).\n\n",
             error);
    exit (100);
  }    

}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                               InfoToHTML
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

void InfoToHTML
    (FILE          *stream,
     const char    *pInfoFile,
     const char    *pNodeName,
     const char    *pUriPrefix,
     const char    *pStylesheet,
     const char    *pContent,
     int            cbContent)

{
  int t, iLine, nLines, iFirst, iLast, length, cbMax;
  int cbAttributes, nTitles = 0;
  const char *pText;
  char *pHtml;
  bool fIsFootnote;
  TEXTATTRIBUTES *pAttributes;
  INFOLINE *pLines;
  INFONODETYPE NodeType;
  NAVIGATIONLINKS NavLinks;


  const HTMLFORMATINFO FormatInfo
           = {
               .pWebLinkAttrs      = "RefType=\"uri\"", 
               .pManPageLinkAttrs  = "RefType=\"manpage\"", 
               .pInfoLinkAttrs     = "RefType=\"info\"", 
               .pInfoContextName   = pInfoFile,
               .pUriPrefix         = pUriPrefix
             };


  NodeType = NodeTypeFromName (pNodeName);

  ClassifyLines (pContent, cbContent, &pLines, &nLines);


  iLine = 0;
  while ((iLine < nLines) 
            && (pLines [iLine].type == INFO_LINE_IGNORABLE))
  {
    iLine++;
  }


  ParseHeaderLine 
        (pContent + pLines [iLine].StartOffset,
         pLines [iLine].EffectiveLength,
         pUriPrefix,
         pInfoFile,
         &NavLinks);

  iLine++;


  fprintf (stream, 
           "<!DOCTYPE html>\n\n"
           "<html>\n"
           "<head>\n"
           "<title>%s</title>\n"
           "<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n"
           "<base href=\"%s\">\n"
           "<style>\n%s\n</style>\n"
           "</head>\n"
           "<body Type=\"info\">\n",
           pNodeName, 
           pUriPrefix,
           pStylesheet);

 
  GenerateNavBar (stream, &NavLinks, &FormatInfo);


  fprintf (stream, "<div id=\"Main\">\n");


  if (NodeType == INFO_NODE_DIRECTORY)
  {
    GenerateTitle (stream, "Info Directory", -1, '*', true);
    nTitles = 1;
  }


  while (iLine < nLines)
  {
    fIsFootnote = false;
    switch (pLines [iLine].type)
    {
      case INFO_LINE_FOOTNOTES:
        iLine++;
        while ((iLine < nLines) 
                   && (pLines [iLine].type == INFO_LINE_BLANK))
        {
          iLine++;
        }  

        if (iLine >= nLines)
          break;

        fIsFootnote = true;


      case INFO_LINE_TEXT:
        iFirst = iLine;
        while ((iLine < nLines)
                  && ((pLines [iLine].type == INFO_LINE_TEXT)
                        || (pLines [iLine].type == INFO_LINE_BLANK)))
        {
          iLine++;
        }
        iLast = iLine - 1;
  
        length = pLines [iLast].EndOffset - pLines [iFirst].StartOffset;
        pText = pContent + pLines [iFirst].StartOffset;
   
        cbAttributes = sizeof (TEXTATTRIBUTES) * length;
        pAttributes = (TEXTATTRIBUTES*) malloc (cbAttributes);
        memset (pAttributes, 0, cbAttributes);
  
        RecognizeLinks (NodeType, pText, length, pAttributes);

        cbMax = length * HTML_EXPANSION_FACTOR;
        pHtml = (char*) malloc (cbMax);
        HTMLizeText (pHtml, cbMax, pText, length, pAttributes, &FormatInfo, 0);

        fprintf (stream, "%s<pre>\n%s\n</pre>\n%s", 
                 fIsFootnote ? "<div class=\"InfoFootnotes\">\n" : "",
                 pHtml,
                 fIsFootnote ? "</div>\n" : "");
        free (pHtml);
        free (pAttributes);
        break;


      case INFO_LINE_TITLE:
        GenerateTitle (stream,
                       pContent + pLines [iLine].StartOffset,
                       pLines [iLine].EffectiveLength,
                       pLines [iLine].UnderlineChar,
                       ++nTitles == 1);
  
        iLine++;
        break;


      default:  
        iLine++;
    }               
  }

  fprintf (stream, "</div>\n");


  fprintf (stream, "\n\n<script>\n\"use strict\";\n");

  fprintf (stream, "%s", ScriptCode);

  GenerateScript (stream, &NavLinks, &FormatInfo);

  fprintf (stream, 
           "</script>\n\n"
           "</body>\n"
           "</html>\n");


  free (pLines);
  free (NavLinks.pPreviousNode);
  free (NavLinks.pPreviousNodeUri);
  free (NavLinks.pNextNode);
  free (NavLinks.pNextNodeUri);
  free (NavLinks.pUpNode);
  free (NavLinks.pUpNodeUri);
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                       EncodeInfoNodeName
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

char* EncodeInfoNodeName
    (const char  *pStr,
     int          length)

{
  int i, n;
  char *pStrDest;
  unsigned char c;
  bool fSpace = false;

  static const char HexDigits [] = "0123456789abcdef";


  if (length < 0)
  {
    length = strlen (pStr);
  }

  
  pStrDest = (char*) malloc (length * 3 + 1);

  for (i = n = 0; i < length; i++)
  {
    c = (unsigned char) pStr [i];
    if (c <= 32)
    {
      fSpace = true;
      continue;
    }

    if (fSpace)
    {
      if (n > 0)
      {  
        pStrDest [n++] = '_';
      }
      fSpace = false;
    }

    if ((c == HEX_ESCAPE_CHAR)
          || (c == '_')
          || (c == '?')
          || (c == '/')
          || (c == '%')
          || (c == '&')
          || (c == '#'))
    {
      pStrDest [n++] = HEX_ESCAPE_CHAR;
      pStrDest [n++] = HexDigits [c >> 4];
      pStrDest [n++] = HexDigits [c & 0x0F];
    }
    else
    {
      pStrDest [n++] = (char) c;
    }  
  }

  pStrDest [n] = '\0';

  return (char*) realloc (pStrDest, n + 1);
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                       DecodeInfoNodeName
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

char* DecodeInfoNodeName
    (const char  *pStr,
     int          length)

{
  int i, n, inc, digit1, digit2;
  char c, *pStrDest;


  if (length < 0)
  {
    length = strlen (pStr);
  }


  pStrDest = (char*) malloc (length + 1);

  for (i = n = 0; i < length; i += inc)
  {
    inc = 1;
    c = pStr [i];
    switch (c)
    {
      case '_':
        pStrDest [n++] = ' ';
        break;

      case HEX_ESCAPE_CHAR:
        if ((i <= length - 3)
               && ((digit1 = ValueFromHexDigit (pStr [i + 1])) >= 0)
               && ((digit2 = ValueFromHexDigit (pStr [i + 2])) >= 0))
        {
          pStrDest [n++] = (char) ((digit1 << 4) + digit2);
          inc = 3;
          break;
        }

      default:
        pStrDest [n++] = c;  
    }    
  }

  pStrDest [n] = '\0';

  return (char*) realloc (pStrDest, n + 1);
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                        ValueFromHexDigit
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
int ValueFromHexDigit
   (char  c)

{
  if ((c >= '0') && (c <= '9'))
    return (int) (c - '0');

  if ((c >= 'a') && (c <= 'f'))
    return (int) (c - ('a' - 10));

  if ((c >= 'A') && (c <= 'F'))
    return (int) (c - ('A' - 10));

  return -1;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                         NodeTypeFromName
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
INFONODETYPE NodeTypeFromName
   (const char   *pNodeName)

{
  if (strcasecmp (pNodeName, "(dir)") == 0)
  	return INFO_NODE_DIRECTORY;


  int length = strlen (pNodeName);

  if ((length >= 5) 
  	    && (strcasecmp (pNodeName + length - 5, "Index") == 0))
    return INFO_NODE_INDEX;


  return INFO_NODE_NORMAL;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                            ClassifyLines
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void ClassifyLines
   (const char   *pContent,
   	int           cbContent,
    INFOLINE    **ppLinesOut,
    int          *pnLinesOut)

{
  int nLines, nMaxLines, length, EffectiveLength;
  int offset, cbPrevLine, cbRemaining;
  bool fHeaderLineFound = false;
  char c;
  const char *pCurrentLine, *pNextLine, *pPrevLine;
  INFOLINE *pLines;


  nLines = 0;
  nMaxLines = 16;
  pLines = (INFOLINE*) malloc (sizeof (INFOLINE) * nMaxLines);

  
  for (pNextLine = pContent
         ; (pCurrentLine = pNextLine) != NULL
         ; nLines++)
  {
    offset = pCurrentLine - pContent;
  	cbRemaining = cbContent - offset;
    pNextLine = (const char*) memchr (pCurrentLine, '\n', cbRemaining);
    length = (pNextLine == NULL)
                ? cbRemaining
                : (pNextLine++ - pCurrentLine);


    EffectiveLength = length;
    while ((EffectiveLength > 0) 
              && (c = pCurrentLine [EffectiveLength - 1], 
                    ((c == ' ') || (c == '\t')
                      || (c == '\n') || (c == '\r'))))
    {
      EffectiveLength--;
    }


    if (nLines == nMaxLines)
    {
      nMaxLines *= 2;
      pLines = (INFOLINE*) realloc (pLines, sizeof (INFOLINE) * nMaxLines);
    }

 
    pLines [nLines].UnderlineChar    = '\0';
    pLines [nLines].StartOffset      = offset;
    pLines [nLines].EndOffset        = offset + length + 1;
    pLines [nLines].EffectiveLength  = EffectiveLength;


    if (EffectiveLength == 0)
    {
      pLines [nLines].type = fHeaderLineFound ? INFO_LINE_BLANK : INFO_LINE_IGNORABLE;
      continue;
    }


    if (IsFootnotesLine (pCurrentLine, EffectiveLength))
    {
      pLines [nLines].type = fHeaderLineFound ? INFO_LINE_FOOTNOTES : INFO_LINE_IGNORABLE;
      continue;
    }


    if ((nLines >= 1)
           && (pLines [nLines - 1].type == INFO_LINE_TEXT)
           && IsUnderline (pCurrentLine, EffectiveLength))
    {
      pPrevLine = pContent + pLines [nLines - 1].StartOffset;
      cbPrevLine = pLines [nLines - 1].EffectiveLength;

      if (CountCharsUTF8 (pPrevLine, cbPrevLine) == EffectiveLength)
      {
        pLines [nLines - 1].type           = INFO_LINE_TITLE;
        pLines [nLines - 1].UnderlineChar  = pCurrentLine [EffectiveLength - 1];

        pLines [nLines].type = INFO_LINE_IGNORABLE;
        continue;
      }
    }


    if (!fHeaderLineFound)
    {
      fHeaderLineFound = true;      
      pLines [nLines].type = INFO_LINE_HEADER;
      continue;
    }


    pLines [nLines].type = INFO_LINE_TEXT;
  }


  *ppLinesOut = (INFOLINE*) realloc (pLines, sizeof (INFOLINE) * nLines);
  *pnLinesOut = nLines;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                              IsUnderline
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
bool IsUnderline
   (const char  *pText,
    int          length)

{
  int i;
  char c1;


  if (length <= 2)
    return false;


  c1 = pText [0];
  if (!((c1 == '*') || (c1 == '.') || (c1 == '-') || (c1 == '=')))
    return false;

  i = 1;
  while ((i < length) && (pText [i] == c1))
  {
    i++;
  }

  return i >= length;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                          IsFootnotesLine
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
bool IsFootnotesLine
   (const char  *pText,
    int          length)

{
  int i, j;
  char c, buffer [16];


  if (length < 9)
    return false;


  for (i = j = 0; (i < length) && (j <= 10); i++)
  {
    c = pText [i];
    if ((c != '-') && (c != ' ') && (c != '\t'))
    {
      buffer [j++] = ((c >= 'a') && (c <= 'z')) ? (c - ('a' - 'A')) : c;
    }
  }

  return (j == 9) && (memcmp (buffer, "FOOTNOTES", 9) == 0);
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                          ParseHeaderLine
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void ParseHeaderLine
   (const char       *pText,
    int               cbText,
    const char       *pPrefix,
    const char       *pContextName,
    NAVIGATIONLINKS  *pLinksOut)

{
  int length;
  const char *pLabel, *pComma;
  char *pNodeName, *pEncodedName, *pUri;


  pLinksOut->pPreviousNode     = NULL;
  pLinksOut->pPreviousNodeUri  = NULL;
  pLinksOut->pNextNode         = NULL;
  pLinksOut->pNextNodeUri      = NULL;
  pLinksOut->pUpNode           = NULL;
  pLinksOut->pUpNodeUri        = NULL;


  if ((cbText > 0)
        && ((pLabel = (const char*) memmem (pText, cbText, "Next: ", 6)) != NULL))
  {
    pLabel += 6;
    pComma = (const char*) memchr (pLabel, ',', cbText - (pLabel - pText));
    cbText = (pComma == NULL) ? 0 : (cbText - (pComma - pText) - 1);
    pText = (pComma == NULL) ? NULL : (pComma + 1);

    length = (pComma == NULL) ? (cbText - (pLabel - pText)) : (pComma - pLabel);
    pNodeName = strndup (pLabel, length); 


    pEncodedName = EncodeInfoNodeName (pNodeName, -1);
    asprintf (&pUri, "%sinfo/%s/%s", pPrefix, pContextName, pEncodedName);
    free (pEncodedName);

    pLinksOut->pNextNode     = pNodeName;
    pLinksOut->pNextNodeUri  = pUri;
  }


  if ((cbText > 0) 
        && ((pLabel = (const char*) memmem (pText, cbText, "Prev: ", 6)) != NULL))
  {
    pLabel += 6;
    pComma = (const char*) memchr (pLabel, ',', cbText - (pLabel - pText));
    cbText = (pComma == NULL) ? 0 : (cbText - (pComma - pText) - 1);
    pText = (pComma == NULL) ? NULL : (pComma + 1);

    length = (pComma == NULL) ? (cbText - (pLabel - pText)) : (pComma - pLabel);
    pNodeName = strndup (pLabel, length); 


    pEncodedName = EncodeInfoNodeName (pNodeName, -1);
    asprintf (&pUri, "%sinfo/%s/%s", pPrefix, pContextName, pEncodedName);
    free (pEncodedName);

    pLinksOut->pPreviousNode     = pNodeName;
    pLinksOut->pPreviousNodeUri  = pUri;
  }


  if ((cbText > 0) 
        && ((pLabel = (const char*) memmem (pText, cbText, "Up: ", 4)) != NULL))
  {
    pLabel += 4;

    length = cbText - (pLabel - pText);
    pNodeName = strndup (pLabel, length); 


    pEncodedName = EncodeInfoNodeName (pNodeName, -1);
    asprintf (&pUri, "%sinfo/%s/%s", pPrefix, pContextName, pEncodedName);
    free (pEncodedName);

    pLinksOut->pUpNode     = pNodeName;
    pLinksOut->pUpNodeUri  = pUri;
  }
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                           RecognizeLinks
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void RecognizeLinks
   (INFONODETYPE     NodeType,
   	const char      *pText,
    int              length,
    TEXTATTRIBUTES  *pAttributes)

{
  switch (NodeType)
  {
  	case INFO_NODE_NORMAL:
      RecognizeURIs (pText, length, pAttributes);
      RecognizeInfoLinks (pText, length, pAttributes);
  	  break;

  	case INFO_NODE_INDEX:
  	  RecognizeInfoIndexLinks (pText, length, pAttributes);
      break;

    case INFO_NODE_DIRECTORY:
      RecognizeDirLinks (pText, length, pAttributes);
  }
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                       RecognizeInfoLinks
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
int RecognizeInfoLinks
   (const char      *pText,
    int              length,
    TEXTATTRIBUTES  *pAttributes)

{
  int i, index, iStart, iEnd, count;
  regmatch_t match [2];


  if (length < 0)
  {
    length = strlen (pText);
  }


  index = count = 0; 
  while ((index < length)
            && !tre_regnexec (&LinkRegex1, pText + index, length - index, 2, match, 0))
  {
    iStart = index + match [1].rm_so;
    iEnd = index + match [1].rm_eo;

    for (i = iStart; i < iEnd; i++)
    {
      pAttributes [i] = (pAttributes [i] & ~TEXT_ATTR_APPEARANCE_MASK)
                            | TEXT_ATTR_INFO_LINK;
    }

    count++;
    index += match [0].rm_eo;
  }

  return count;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                  RecognizeInfoIndexLinks
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
int RecognizeInfoIndexLinks
   (const char      *pText,
    int              length,
    TEXTATTRIBUTES  *pAttributes)

{
  int i, index, iStart, iEnd, count, flags;
  int iStart2, iEnd2;
  bool fValidLink;
  regmatch_t match [3];


  if (length < 0)
  {
    length = strlen (pText);
  }


  index = count = flags = 0;
  
  while ((index < length)
              && !tre_regnexec (&LinkRegex2, pText + index, length - index, 3, match, flags))
  {
    iStart = index + match [1].rm_so;
    iEnd = index + match [1].rm_eo;


    fValidLink = true;
    for (i = iStart; i < iEnd; i++)
    {
      if (pAttributes [i] & TEXT_ATTR_URI)
      {
        fValidLink = false;
        break;
      }
    }


    if (fValidLink)
    {
      for (i = iStart; i < iEnd; i++)
      {
        pAttributes [i] = (pAttributes [i] & ~TEXT_ATTR_APPEARANCE_MASK)
                              | TEXT_ATTR_INFO_LINK;
      }
  
      iStart2 = index + match [2].rm_so;
      iEnd2 = index + match [2].rm_eo;
      for (i = iStart2; i < iEnd2; i++)
      {
        pAttributes [i] |= TEXT_ATTR_INFO_TARGET;
      }
  
      count++;
    }


    index += match [0].rm_eo;
    flags = REG_NOTBOL;
  }

  return count;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                        RecognizeDirLinks
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
int RecognizeDirLinks
   (const char      *pText,
    int              length,
    TEXTATTRIBUTES  *pAttributes)

{
  int i, index, iStart, iEnd, count, flags;
  bool fValidLink;
  regmatch_t match [4];


  if (length < 0)
  {
    length = strlen (pText);
  }


  index = count = flags = 0;
  
  while ((index < length)
              && !tre_regnexec (&DirLinkRegex, pText + index, length - index, 4, match, flags))
  {
    iStart = index + match [0].rm_so;
    iEnd = index + match [0].rm_eo;
    fValidLink = true;
    for (i = iStart; i < iEnd; i++)
    {
      if (pAttributes [i] & TEXT_ATTR_URI)
      {
        fValidLink = false;
        break;
      }
    }


    if (fValidLink)
    {
      iStart = index + match [1].rm_so;
      iEnd = index + match [3].rm_eo;
      for (i = iStart; i < iEnd; i++)
      {
        pAttributes [i] |= TEXT_ATTR_INFO_LINK;
      }

      iStart = index + match [2].rm_so;
      iEnd = index + match [2].rm_eo;
      for (i = iStart; i < iEnd; i++)
      {
        pAttributes [i] |= TEXT_ATTR_INFO_FILENAME;
      }
  
      iStart = index + match [3].rm_so;
      iEnd = index + match [3].rm_eo;
      for (i = iStart; i < iEnd; i++)
      {
        pAttributes [i] |= TEXT_ATTR_INFO_TARGET;
      }
  
      count++;
    }


    index += match [0].rm_eo;
    flags = REG_NOTBOL;
  }

  return count;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                           GenerateNavBar
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void GenerateNavBar
   (FILE                   *stream,
    const NAVIGATIONLINKS  *pNavLinks,
    const HTMLFORMATINFO   *pFormat)

{
  char buffer [256];


  fprintf (stream,
           "<div class=\"NavBar\">\n");


  /*  Previous node link.
  */

  if ((pNavLinks->pPreviousNode == NULL)
         || (pNavLinks->pPreviousNodeUri == NULL))
  {
    fprintf (stream, 
             "<div id=\"Nav-Info-Prev\">\n"
             "<span Label=\"1\">Prev:</span>none\n"
             "</div>\n");
  }
  else
  {
    HTMLizeText (buffer, sizeof (buffer), pNavLinks->pPreviousNode, -1, NULL, NULL, 0);

    fprintf (stream, 
             "<div id=\"Nav-Info-Prev\">\n"
             "<span Label=\"1\">Prev:</span><a href=\"%s\">%s</a>\n"
             "</div>\n",           
             pNavLinks->pPreviousNodeUri,
             buffer);
  }


  /*  Up node link.
  */

  if ((pNavLinks->pUpNode == NULL)
         || (pNavLinks->pUpNodeUri == NULL))
  {
    fprintf (stream, 
             "<div id=\"Nav-Info-Up\">\n"
             "<span Label=\"1\">Up:</span>none\n"
             "</div>\n");
  }
  else
  {
    HTMLizeText (buffer, sizeof (buffer), pNavLinks->pUpNode, -1, NULL, NULL, 0);

    fprintf (stream, 
             "<div id=\"Nav-Info-Up\">\n"
             "<span Label=\"1\">Up:</span><a href=\"%s\">%s</a>\n"
             "</div>\n",           
             pNavLinks->pUpNodeUri,
             buffer);
  }


  /*  Next node link.
  */

  if ((pNavLinks->pNextNode == NULL)
         || (pNavLinks->pNextNodeUri == NULL))
  {
    fprintf (stream, 
             "<div id=\"Nav-Info-Next\">\n"
             "<span Label=\"1\">Next:</span>none\n"
             "</div>\n");
  }
  else
  {
    HTMLizeText (buffer, sizeof (buffer), pNavLinks->pNextNode, -1, NULL, NULL, 0);

    fprintf (stream, 
             "<div id=\"Nav-Info-Next\">\n"
             "<span Label=\"1\">Next:</span><a href=\"%s\">%s</a>\n"
             "</div>\n",           
             pNavLinks->pNextNodeUri,
             buffer);
  }

  fprintf (stream, "</div>\n\n");
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                           GenerateScript
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void GenerateScript
   (FILE                   *stream,
    const NAVIGATIONLINKS  *pNavLinks,
    const HTMLFORMATINFO   *pFormat)

{
  if ((pNavLinks->pPreviousNodeUri == NULL)
        && (pNavLinks->pNextNodeUri == NULL)
        && (pNavLinks->pUpNodeUri == NULL))
    return;


  fprintf (stream, 
           "function HandleKeyPress\n"
           "    (event)\n"
           "{\n"
           "  if (!event || event.isComposing || event.altKey || event.ctrlKey)\n"
           "    return;\n"
           "  switch (event.which || event.keyCode)\n"
           "  {\n");

  if (pNavLinks->pPreviousNodeUri != NULL)
  {
    fprintf (stream, 
             "    case 91:\n"
             "      window.open (\"%s\", \"_self\");\n"
             "      break;\n",
             pNavLinks->pPreviousNodeUri);
  }	

  if (pNavLinks->pNextNodeUri != NULL)
  {
    fprintf (stream, 
             "    case 93:\n"
             "      window.open (\"%s\", \"_self\");\n"
             "      break;\n",
             pNavLinks->pNextNodeUri);
  }	

  if (pNavLinks->pUpNodeUri != NULL)
  {
    fprintf (stream, 
             "    case 61:\n"
             "      window.open (\"%s\", \"_self\");\n"
             "      break;\n",
             pNavLinks->pUpNodeUri);
  }	

  fprintf (stream, 
           "  }\n"
           "}\n\n"
           "document.addEventListener (\"keypress\", HandleKeyPress, false);\n"); 
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                            GenerateTitle
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

static
void GenerateTitle
   (FILE        *stream,
    const char  *pText,
    int          length,
    char         cUnderline,
    bool         fIsFirst)

{
  const char *pULTypeAttr;
  char NumberStr [64], buffer [128];
  regmatch_t match [2];


  if (length < 0)
  {
    length = strlen (pText);
  }


  if (!tre_regnexec (&TitleRegex, pText, length, 2, match, 0))
  {
    snprintf (NumberStr, sizeof (NumberStr), 
              "<span Number=\"1\">%.*s</span>",
              match [1].rm_eo - match [1].rm_so, 
              pText + match [1].rm_so);

    pText += match [0].rm_eo;
    length -= match [0].rm_eo;
  }
  else
  {
    NumberStr [0] = '\0';
  }


  HTMLizeText (buffer, sizeof (buffer), pText, length, NULL, NULL, 0);

  switch (cUnderline)
  {
    case '-':  pULTypeAttr = " ULType=\"dash\"";    break;
    case '.':  pULTypeAttr = " ULType=\"dot\"";     break;
    case '*':  pULTypeAttr = " ULType=\"star\"";    break;
    case '=':  pULTypeAttr = " ULType=\"equals\"";  break;
    default:   pULTypeAttr = "";
  }

  fprintf (stream,
           "<div class=\"InfoTitle\"%s%s%s>\n%s%s\n</div>\n\n",
           pULTypeAttr,
           (NumberStr [0] == '\0') ? "" : " HasNumber=\"\"",
           fIsFirst ? " First=\"\"" : "",
           NumberStr,
           buffer);
}
