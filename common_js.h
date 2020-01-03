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



#ifndef __COMMON_JS_H_
#define __COMMON_JS_H_



const char AdjustMarginCode []
        = "/*----------------------------*/\n"
          "const MainDiv = document.getElementById (\"Main\");\n"
          "const Navbar = document.getElementById (\"NavBar\");\n"
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
          "window.addEventListener (\"resize\", AdjustTopMargin, false);\n"
          "/*----------------------------*/\n\n";



#endif
