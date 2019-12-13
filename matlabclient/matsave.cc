// copied from matlab: edit([matlabroot '/extern/examples/eng_mat/matcreat.c']);
// set LD_LIBRARY_PATH: export LD_LIBRARY_PATH=/usr/local/MATLAB/R2018b/bin/glnxa64/
// commandline -L doesn't seem to cut it.
// COMPILES WITH: gcc matsaver.cc -I /usr/local/MATLAB/R2018a/extern/include/ -L /usr/local/MATLAB/R2018b/bin/glnxa64/ -lmat -lmx -lzmq


#include <stdio.h>
#include <string.h> /* For strcmp() */
#include <stdlib.h> /* For EXIT_FAILURE, EXIT_SUCCESS */
#include <complex>
#include "mat.h"

#include <time.h>
#include <zmq.h>

#define BUFFER_LENGTH 1<<20

int8_t *buffer;

void *zmq_context;
void *socketr;

int initzmq(const char *addr, const char *port){
  char addrstr[128];
  int timeout = 500;
  

  zmq_context = zmq_init(1);
  socketr = zmq_socket(zmq_context,ZMQ_SUB);

  //sampledata socket:
  sprintf(addrstr,"tcp://%s:%s",addr,port);
  zmq_setsockopt(socketr, ZMQ_SUBSCRIBE, "", 0 );
  zmq_setsockopt(socketr,ZMQ_RCVTIMEO,&timeout,sizeof(int));
  zmq_connect(socketr,addrstr);

  buffer = (int8_t*) malloc(BUFFER_LENGTH);
  if (buffer==NULL)
    return -1;

  return 0;
}

int main(int argc, char **argv) {

  if (initzmq("127.0.0.1","5555"))
    return -1;

  int nreceived = zmq_recv(socketr,buffer,BUFFER_LENGTH, 0);
        
    if (nreceived > 0 ){
      uint32_t gseq = *((uint32_t *) buffer);
      uint32_t c    = *(((uint32_t *) buffer)+1);
      uint32_t r    = *(((uint32_t *) buffer)+2);

      float scaler=1.0f/128.0f;
      int offset=16+sizeof(uint32_t)*c;

      MATFile *pmat;
      mxArray *pa1,*pa3;
      const char *file = "mattest.mat";
      printf("Creating file %s...\n\n", file);
      pmat = matOpen(file, "w");

      pa1 = mxCreateNumericMatrix(r,c,mxSINGLE_CLASS,mxCOMPLEX);

      time_t t = time(NULL);
      struct tm *tm = localtime(&t);
      char s[64];
      strftime(s, sizeof(s), "%c", tm);
      pa3 = mxCreateString(s);


      float* outptr = (float *) mxGetComplexSingles(pa1);
          
          //convert sampledata to a matrix of complex float:
          for(int n=0; n< ((r*c)<<1); n++)
            outptr[n] = scaler*buffer[offset+n];

      matPutVariable(pmat, "sampledata", pa1);
      matPutVariable(pmat, "time", pa3);
      mxDestroyArray(pa1);
      mxDestroyArray(pa3);
      matClose(pmat);
      }


  zmq_close(socketr);
  free(buffer);

/*

  MATFile *pmat;
  mxArray *pa1, *pa2, *pa3;
  double data[9] = { 1.0, 4.0, 7.0, 2.0, 5.0, 8.0, 3.0, 6.0, 9.0 };
  const char *file = "mattest.mat";
  char str[BUFSIZE];
  int status; 

  printf("Creating file %s...\n\n", file);
  pmat = matOpen(file, "w");
  if (pmat == NULL) {
    printf("Error creating file %s\n", file);
    printf("(Do you have write permission in this directory?)\n");
    return(EXIT_FAILURE);
  }

  pa1 = mxCreateNumericMatrix(3,3,mxSINGLE_CLASS,mxCOMPLEX);
  if (pa1 == NULL) {
      printf("%s : Out of memory on line %d\n", __FILE__, __LINE__); 
      printf("Unable to create mxArray.\n");
      return(EXIT_FAILURE);
  }
  std::complex<float> *p = (std::complex<float>*) mxGetComplexSingles(pa1); //this gets a pointer correctly.
  for (int n=0;n<9;n++)
    p[n] = std::complex<float>(n,n);
  //memcpy((void *)(mxGetPr(pa1)), (void *)data, sizeof(data));

  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  char s[64];
  strftime(s, sizeof(s), "%c", tm);
  pa3 = mxCreateString(s);
  //pa3 = mxCreateString("MATLAB: the language of technical computing");
  if (pa3 == NULL) {
      printf("%s :  Out of memory on line %d\n", __FILE__, __LINE__);
      printf("Unable to create string mxArray.\n");
      return(EXIT_FAILURE);
  }

  status = matPutVariable(pmat, "sampledata", pa1);
  if (status != 0) {
      printf("%s :  Error using matPutVariable on line %d\n", __FILE__, __LINE__);
      return(EXIT_FAILURE);
  }  

  status = matPutVariable(pmat, "time", pa3);
  if (status != 0) {
      printf("%s :  Error using matPutVariable on line %d\n", __FILE__, __LINE__);
      return(EXIT_FAILURE);
  } 
  
  mxDestroyArray(pa1);
  
  mxDestroyArray(pa3);

  if (matClose(pmat) != 0) {
    printf("Error closing file %s\n",file);
    return(EXIT_FAILURE);
  }

  printf("Done\n");
  return(EXIT_SUCCESS);
  */
}