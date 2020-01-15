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

#include "utility.h"                   /*  Application headers.  */
#include "html_formatting.h"
#include "apropostohtml.h"
#include "installation.h"
#include "common_js.h"



/*  Dynamically-generated headers.
*/

#include "dynamic/apropos_script.h"



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                     AproposResultsToHTML
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

void AproposResultsToHTML
    (FILE                 *stream,
     const char           *pKeyword,
     const char           *pUriPrefix,
     const char           *pStylesheet,
     const APROPOSRESULT  *pResults,
     int                   nResults)

{
  int i;
  char buffer [256];
  char id [32], description [128];


  HTMLizeText (buffer, sizeof (buffer), pKeyword, -1, NULL, NULL, 0);

  fprintf (stream,
           "<!DOCTYPE html>\n\n"
           "<html>\n"
           "<head>\n"
           "<title>Apropos: %s</title>\n"
           "<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n"
           "<base href=\"%s\">\n"
           "<style>\n%s\n</style>\n"
           "</head>\n"
           "<body Type=\"apropos\">\n",
           buffer, 
           pUriPrefix,
           pStylesheet);


  fprintf (stream, 
           "<div id=\"NavBar\">\n"
           "<div id=\"Nav-Apropos-NResults\"></div>\n"
           "<div id=\"Nav-Apropos-Sort\">\n"
           "<span Label=\"1\">Sort by:</span>"
           "<span id=\"ByNameButton\" class=\"Button FormatSelector\">Name</span>\n"
           "&nbsp;\n"
           "<span id=\"BySectionButton\" class=\"Button FormatSelector\">Section</span>\n"
           "</div>\n"
           "<div id=\"Nav-Apropos-ShowHide\">\n"
           "<span id=\"ShowAllButton\" class=\"Button\">Show all</span>\n"
           "&nbsp;\n"
           "<span id=\"HideAllButton\" class=\"Button\">Hide all</span>\n"
           "</div>\n"           
           "</div>\n");


  fprintf (stream, 
           "<div id=\"Main\">\n"
           "<div id=\"Results\"></div>\n"           
           "</div>\n\n\n");

  fprintf (stream, "<script>\n\"use strict\";\n");
  fprintf (stream, "const UriPrefix = \"%s\";\n\n", pUriPrefix);

  fprintf (stream, "%s", AdjustMarginCode);


  fprintf (stream, "const SectionTitles =\n{\n");
  for (i = 0; ManualSections [i] != NULL; i += 2)
  {
    JSEscapeString (ManualSections [i], id, sizeof (id));
    JSEscapeString (ManualSections [i + 1], description, sizeof (description));

    fprintf (stream, "%s   \"%s\": \"%s\"", 
             (i == 0) ? "" : ",\n",
             id, description);
  }

  fprintf (stream, "\n};\n\n\n");


  fprintf (stream, "\nlet results =\n[");

  for (i = 0; i < nResults; i++)
  {
    JSEscapeString (pResults [i].pDescription, buffer, sizeof (buffer));

    if ((buffer [0] >= 'a') && (buffer [0] <= 'z'))
    {
      buffer [0] -= 'a' - 'A';
    }

  	fprintf (stream, "[\"%s\", \"%s\", \"%s\"]%s\n",
  		       pResults [i].pPageTitle,
  		       pResults [i].pSection,
  		       buffer,
  		       (i == nResults - 1) ? "];\n\n" : ",");
  } 


  fprintf (stream, "%s</script>\n</body>\n</html>\n",
           ScriptCode);
}


