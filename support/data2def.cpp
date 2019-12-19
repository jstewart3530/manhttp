#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>



#define MAX_LINE_LENGTH            78



void CaptureInput (int, void**, int*, int);
int CreateDefinition (FILE*, const void*, int, const char*);



int main
   (int     argc,
    char  **argv)

{
  int cbData;
  void *pData;


  if (argc < 2)
  {
    printf ("\nUsage:  %s variable-name\n\n"
            "Input is read from stdin and output is sent to stdout.\n\n",
            argv [0]);
    return 0;
  }


  CaptureInput (0, &pData, &cbData, 0);

  if (cbData <= 0)
    return 1;


  CreateDefinition (stdout, pData, cbData, argv [1]);
  printf ("\n\n");

  free (pData);

  return 0;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                         CreateDefinition
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

int CreateDefinition 
   (FILE         *stream, 
    const void   *pData, 
    int           cbData, 
    const char   *pVariableName)

{
  int i, j, nDigits, nLines, nIndent, length;
  unsigned int v;
  char line [MAX_LINE_LENGTH];


  fprintf (stream, "static const unsigned char %s [%d]\n", 
           pVariableName, 
           cbData);


  strcpy (line, "   = {");
  nIndent = length = 6;
  nLines = 0;

  for (i = 0; i < cbData; i++)
  {
    v = (unsigned int) ((unsigned char*) pData) [i];


    if (length > nIndent)
    {
      line [length++] = ',';
      line [length++] = ' ';  
    }


    if (v <= 9)
    {
      nDigits = 1;
    }
    else if (v <= 99)
    {
      nDigits = 2;
    }
    else
    {
      nDigits = 3;
    }

    length += nDigits;    
    for (j = 1; j <= nDigits; j++)
    {
      line [length - j] = '0' + (char) (v % 10);
      v /= 10;
    }

    
    if ((MAX_LINE_LENGTH - length < 5)
           || (i == cbData - 1))
    {
      fprintf (stream, "%.*s%s\n",
               length, line, ((i == cbData - 1) ? "};" : ","));

      if (++nLines == 1)
      {
        memset (line, ' ', nIndent);
      }

      length = nIndent;
    }
  }

  return nLines + 1;
}



/*-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-
                                                             CaptureInput
-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-*/

void CaptureInput
   (int      fd,
    void   **ppDataOut,
    int     *pcbDataOut,
    int      cbMaxData)

{
  int i, cbRead, cbReadSoFar, cbBuffer;
  char *pBuffer;
  struct pollfd pfd;


  /*  Allocate a buffer.
  */

  cbRead = cbReadSoFar = 0;
  cbBuffer = (cbMaxData <= 0) ? 1024 : (cbMaxData + 1);
  pBuffer = (char*) malloc (cbBuffer);


  /*  Read data into the buffer until (1) an error or zero-length read
  *   occurs, (2) cbMaxData bytes have been read, or (3) poll() reports
  *   a hang-up or error condition on the file descriptor.
  */

  for (;;)
  {
    pfd.fd       = fd;
    pfd.events   = POLLIN;
    pfd.revents  = 0;
    poll (&pfd, 1, -1);

    
    if (pfd.revents & POLLIN)
    {
      if ((cbMaxData <= 0) && (cbReadSoFar >= cbBuffer - 1))
      {
        cbBuffer *= 2;
        pBuffer = (char*) realloc (pBuffer, cbBuffer);
      }

      if ((cbRead = read (fd, pBuffer + cbReadSoFar, cbBuffer - cbReadSoFar)) <= 0)
        break;

      cbReadSoFar += cbRead;

      if ((cbMaxData > 0) && (cbReadSoFar >= cbMaxData))
        break;
    }


    if (pfd.revents & (POLLHUP | POLLERR))
      break;
  }


  /*  Add a terminating zero character.
  */

  pBuffer [cbReadSoFar] = '\0';


  /*  Shrink the buffer if necessary to free any unused space at the end.
  */

  *ppDataOut = (char*) realloc (pBuffer, cbReadSoFar + 1);

  if (pcbDataOut != NULL)
  {
    *pcbDataOut = cbReadSoFar;
  }
}
