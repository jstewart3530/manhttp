/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-

  stringify converts a piece of text into a C/C++ string declaration.

  It reads text from the standard input and writes to the standard
  output:
 
    stringify UIDefinition < foo.glade > ui_definition.h

  The sole argument is the name of the variable being declared.

  To compile:

    gcc -o stringify -g stringify.cpp


  (C) 2018 Jonathan Stewart.
  https://github.com/jstewart3530/stringify
  jstewart3530@gmail.com
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>



#define MAX_INPUT_LINE_LENGTH      1024
#define MAX_OUTPUT_LINE_LENGTH     128



/*  Function prototypes.
*/

int FindLineBreakPoint (const char*, int);
bool EscapeString (char*, int*, int);



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                                     main
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

int main
   (int     argc,
    char  **argv)

{
  int m, n, StartIndex, LineLength, nLines, nBlankLines;
  char c, *pBuffer;


  static const char Newlines []
          = "\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n"
            "\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n";

  static const char Indent []       = "          ",
                    IndentFirst []  = "        = ";   


  if (argc < 2)
  {
    printf ("\nUsage:  %s variable-name\n\n"
            "Input is read from stdin and output is sent to stdout.\n\n",
            argv [0]);
    return 0;
  }


  pBuffer = (char*) malloc (MAX_INPUT_LINE_LENGTH);
  nBlankLines = nLines = 0;


  printf ("\n\nconst char %s []", argv [1]);


  for (;;)
  {
    LineLength = (fgets (pBuffer, MAX_INPUT_LINE_LENGTH, stdin) == NULL)
                    ? -1
                    : strlen (pBuffer);

    while ((LineLength > 0) 
             && (c = pBuffer [LineLength - 1],
                    ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r'))))
    {
      LineLength--;
    }

    if (LineLength == 0)
    {
      nBlankLines++;
      continue;
    }


    for (n = nBlankLines; n > 0; n -= m, nLines++)
    {
      m = (n <= 32) ? n : 32;

      printf ("\n%s\"%.*s\"",
              ((nLines == 0) ? IndentFirst : Indent),
              m * 2, Newlines);
    }

    nBlankLines = 0;


    if (LineLength == -1)
      break;


    EscapeString (pBuffer, &LineLength, MAX_INPUT_LINE_LENGTH);
    pBuffer [LineLength] = '\0';

    for (StartIndex = 0; StartIndex < LineLength; StartIndex += m, nLines++)    
    {
      m = FindLineBreakPoint (pBuffer + StartIndex, MAX_OUTPUT_LINE_LENGTH);

      printf ("\n%s%s\"%.*s%s\"",
              ((nLines == 0) ? IndentFirst : Indent),
              ((StartIndex == 0) ? "" : "  "),
              m, pBuffer + StartIndex,
              ((StartIndex + m >= LineLength) ? "\\n" : ""));
    }
  }  


  printf ((nLines > 0) ? ";\n\n\n" : " = \"\";\n\n\n");


  free (pBuffer);

  return 0;
}




/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                       FindLineBreakPoint
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

int FindLineBreakPoint
   (const char  *pStr,
    int          MaxLength)

{
  int index;
  char c;


  index = 0;
  while ((index < MaxLength) && ((c = pStr [index]) != '\0'))
  {
    index += (c == '\\') ? 2 : 1;
  }

  return index;
}




/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                             EscapeString
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

bool EscapeString
   (char  *pStr,
    int   *pLength,
    int    cbStrMax)

{
  int index, length;
  char c, cEscape;
  bool fSuccess = true;

  
  index = 0;
  length = *pLength;

  while (index < length)
  {
    c = pStr [index];

    switch (c)
    {
      case '\\':  cEscape = '\\';  break;
      case '"':   cEscape = '"';   break;
      case '\t':  cEscape = 't';   break;
      case '\a':  cEscape = 'a';   break;
      case '\b':  cEscape = 'b';   break;
      case '\f':  cEscape = 'f';   break;
      case '\r':  cEscape = 'r';   break;
      case '\v':  cEscape = 'v';   break;
      default:    cEscape = '\0';
    }

    if (cEscape != '\0')
    {
      if (length >= cbStrMax)
      {
        fSuccess = false;
        break;
      }
    
      length++;
      memmove (pStr + index + 1, pStr + index, length - index);
      pStr [index++] = '\\';
      pStr [index] = cEscape;
    }

    index++;
  }


  *pLength = length;

  return fSuccess;
}
