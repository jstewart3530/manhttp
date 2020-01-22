#~--~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
#  manhttp 
#
#  An HTTP server that provides a web-based, hypertext-driven
#  frontend for man(1), info(1), and apropos(1).  MANHTTP lets you
#  browse and search your system's online documentation using a web
#  browser.
#
#~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~
#
#  Copyright 2019 Jonathan Stewart
#  
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#  
#      http://www.apache.org/licenses/LICENSE-2.0
#  
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~


#  Compilation and link settings.

COMPILE = gcc
LINK = gcc
SASS = sassc -t compact
COMPILE_OPTS := -c -g -pipe -Wall
LINK_OPTS = -g -pipe
LIBS := -lstdc++ -ltre -lmicrohttpd -lpopt



ifeq ($(shell stty -g 2> /dev/null),)
	Message = @echo -e "  " $(1) $(2) $(3) $(4) $(5) $(6) $(7) $(8)
	Arrow = "-->"
else
	Message = @echo -e "  \e[34m" $(1) "\e[0m" $(2) $(3) $(4) $(5) $(6) $(7) $(8)
	Arrow = "\e[31;1m-->\e[0m"
endif


define Compile
$(call Message, "Compiling", $(notdir $<), $(Arrow), $(notdir $@))
@$(COMPILE) $(COMPILE_OPTS) -o $@ $<
endef


define SassProcess
$(call Message, "Processing", $(notdir $<), $(Arrow), $(notdir $@))
@$(SASS) $< $@
endef


#  Directory where intermediate files are placed.

INTERMEDIATE_DIR = build_temp


#  Filename for the executable.

EXECUTABLE = manhttp


#  Module names (minus the ".o" at the end).

MODULES = \
    manhttp_main \
    utility \
	man_page_api \
    manualpagetohtml \
	apropostohtml \
	infotohtml \
	html_formatting \
	installation


#  Module-specific compilation options.

#  (None for now.)



#  Build rule for the executable.


O_FILES = $(addprefix $(INTERMEDIATE_DIR)/, $(addsuffix .o, $(MODULES)))

$(EXECUTABLE) : $(O_FILES)
	$(call Message, "Linking", $(notdir $@))
	@$(LINK) $(LINK_OPTS) -o $(EXECUTABLE) $(O_FILES) $(LIBS)
	$(call Message, "Build complete.")


#  Build rule for the intermediate (temp) directory.


$(O_FILES):  | $(INTERMEDIATE_DIR)


$(INTERMEDIATE_DIR) :
	$(call Message, "Creating", "directory", $(INTERMEDIATE_DIR), "for intermediate files")
	@mkdir $(INTERMEDIATE_DIR)


#  Build rules for each module.


$(INTERMEDIATE_DIR)/manhttp_main.o : \
		manhttp_main.cpp  manualpagetohtml.h  apropostohtml.h \
		infotohtml.h  man_page_api.h  utility.h  installation.h \
		dynamic/stylesheet_text.h  dynamic/splash_html.h  dynamic/favicon.h
	$(Compile)

$(INTERMEDIATE_DIR)/utility.o : \
		utility.cpp  utility.h
	$(Compile)

$(INTERMEDIATE_DIR)/manualpagetohtml.o : \
		manualpagetohtml.cpp  manualpagetohtml.h  man_page_api.h \
		html_formatting.h  utility.h  common_js.h \
		dynamic/man_page_script.h
	$(Compile)

$(INTERMEDIATE_DIR)/apropostohtml.o : \
		apropostohtml.cpp  apropostohtml.h  man_page_api.h \
		html_formatting.h  utility.h  common_js.h \
		installation.h  dynamic/apropos_script.h 
	$(Compile)

$(INTERMEDIATE_DIR)/infotohtml.o : \
		infotohtml.cpp  infotohtml.h  man_page_api.h \
		html_formatting.h  utility.h  common_js.h
	$(Compile)

$(INTERMEDIATE_DIR)/man_page_api.o : \
		man_page_api.cpp  man_page_api.h  utility.h
	$(Compile)

$(INTERMEDIATE_DIR)/html_formatting.o : \
		html_formatting.cpp  html_formatting.h
	$(Compile)

$(INTERMEDIATE_DIR)/installation.o : \
		installation.cpp  installation.h
	$(Compile)



#  Build rules for programs used in the build process

stringify : support/stringify.cpp
	$(call Message, "Compiling stringify")
	@$(COMPILE) -o $@ -O2 $<

data2def : support/data2def.cpp
	$(call Message, "Compiling data2def")
	@$(COMPILE) -o $@ -O2 $<



#  Build rules for dynamic headers.

dynamic :
	$(call Message, "Creating", "directory", $@, "for dynamically-generated header files")
	@mkdir $@

dynamic/stylesheet_text.h :  style.css | stringify dynamic
	$(call Message, "Generating", $@)
	@./stringify DefaultStylesheet < $< > $@

dynamic/man_page_script.h :  man_page.js | stringify dynamic
	$(call Message, "Generating", $@)
	@./stringify ScriptCode < $< > $@

dynamic/apropos_script.h :  apropos.js | stringify dynamic
	$(call Message, "Generating", $@)
	@./stringify ScriptCode < $< > $@

dynamic/splash_html.h :  splash.html | stringify dynamic
	$(call Message, "Generating", $@)
	@./stringify SplashText < $< > $@

dynamic/favicon.h :  favicon.ico | data2def dynamic
	$(call Message, "Generating", $@)
	@./data2def FavIcon < $< > $@



# Sass processing

style.css :  style.scss
	$(SassProcess)
	


BUILDALWAYS :


clean :
	rm -rf $(EXECUTABLE) $(INTERMEDIATE_DIR)/* dynamic/*
