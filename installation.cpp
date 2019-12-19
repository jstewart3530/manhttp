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


#include <stdio.h>                    /*  C/C++ RTL headers.  */

#include "installation.h"             /*  Application headers.  */



const char *ManualSections []
     /*   Section    Description                        */
       = {"0",       "Header files",
          "0p",      "Header files (POSIX)",
          "1",       "Programs and commands",
          "1p",      "Programs and commands (POSIX)",
          "2",       "System calls",
          "3",       "Library and API functions",
          "3am",     "GNU AWK",
          "3g",      "OpenGL",
          "3p",      "Library and API functions (POSIX)",
          "3pm",     "Perl",
          "3ssl",    "OpenSSL API",
          "4",       "Devices/Special files",
          "5",       "File formats",
          "6",       "Games and screensavers",
          "6x",      "Games and screensavers",
          "7",       "Miscellaneous",
          "8",       "Administration programs",
          "9",       "Kernel",
          "n",       "Tcl/Tk",
          NULL};               /*  Required NULL terminator--do not remove!  */



/*  Fully-qualified paths for the man(1), apropos(1), and info(1)
*   executables.
*/

const char *ManPath      = "/usr/bin/man";

const char *AproposPath  = "/usr/bin/apropos";

const char *InfoPath     = "/usr/bin/info";

