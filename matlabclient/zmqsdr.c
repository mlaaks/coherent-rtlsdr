/*
coherent-rtlsdr

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

coherent-rtlsdr is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with coherent-rtlsdr.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <zmq.h>
#include "mex.h"
#include "matrix.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define BUFFER_LENGTH 1<<20
#define COMMANDBUFFER_LENGTH 256

void *zmq_context;
void *socketr, *socketc;

int8_t *buffer;
char   *commandbuf;

uint32_t rows,cols;


typedef struct args_t{
            char     op;
            uint32_t centerfreq;
            char     *ip,*port,*cport;
}args_t;

void parseargs(args_t *args,int nrhs ,const mxArray *prhs[]){
	if (nrhs > 0){
		args->op = mxArrayToString(prhs[0])[0];
	}
	if ((nrhs>1) && (args->op=='t')){
		args->centerfreq = *mxGetDoubles(prhs[1]);
	}
	if (nrhs==5){
		args->ip = mxArrayToString(prhs[2]);
		args->port = mxArrayToString(prhs[3]);
		args->cport=mxArrayToString(prhs[4]);
	}
}

int initzmq(char *addr, char *port, char *cport){
	char addrstr[128];
	int timeout = 500;

	
	//mexPrintf("%s\n",addrstr);

	zmq_context = zmq_init(1);
	socketr = zmq_socket(zmq_context,ZMQ_SUB);

	//sampledata socket:
	sprintf(addrstr,"tcp://%s:%s",addr,port);
	zmq_setsockopt(socketr, ZMQ_SUBSCRIBE, "", 0 );
	zmq_setsockopt(socketr,ZMQ_RCVTIMEO,&timeout,sizeof(int));
	zmq_connect(socketr,addrstr);

	//control socket:
	sprintf(addrstr,"tcp://%s:%s",addr,cport);
	socketc = zmq_socket(zmq_context,ZMQ_DEALER);
	zmq_connect(socketc,addrstr);

	buffer = (int8_t*) malloc(BUFFER_LENGTH);
	if (buffer==NULL)
		return -1;
	
	commandbuf = (char*) malloc(COMMANDBUFFER_LENGTH);
	if (commandbuf==NULL)
		return -1;

return 0;
}

void cleanup(void){
	free(buffer);
	free(commandbuf);
	zmq_close(socketr);
	zmq_close(socketc);
	zmq_term(zmq_context);
}

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {

	uint32_t r=8192;
	uint32_t c=22;

	args_t args;
	//mexPrintf("parsing...");
	parseargs(&args,nrhs,prhs);
	//mexPrintf("found %c \n",args.op);

	switch(args.op){
		case 'i':
                plhs[0] = mxCreateNumericMatrix(1,1,mxINT32_CLASS,mxREAL);
                int32_t* ret = (int32_t *) mxGetPr(plhs[0]);
                *ret = initzmq(args.ip,args.port,args.cport);
                mexPrintf("Initialized zmq, %s:%s, buffers allocated\n",args.ip,args.port);
                //mexAtExit(cleanup);  //would cause double free in case we already free from the matlab system object
        break;
        case 'r':{
      		
	        	int nreceived = zmq_recv(socketr,buffer,BUFFER_LENGTH, 0);
				
	        	if (nreceived > 0 ){
		        	uint32_t gseq = *((uint32_t *) buffer);
		          	uint32_t c    = *(((uint32_t *) buffer)+1);
		            uint32_t r    = *(((uint32_t *) buffer)+2);

		            float scaler=1.0f/128.0f;
		            int offset=16+sizeof(uint32_t)*c;
		            
		            plhs[0] = mxCreateNumericMatrix(r,c,mxSINGLE_CLASS,mxCOMPLEX);	
		        	
					//we take the pointer as float, even though our data is interpreted as interleaved complex
					float* outptr = (float *) mxGetComplexSingles(plhs[0]);
					
					//convert sampledata to a matrix of complex float:
					for(int n=0; n< ((r*c)<<1); n++)
						outptr[n] = scaler*buffer[offset+n];

					//fill sequence numbers:
					plhs[1] = mxCreateDoubleScalar(gseq);
					plhs[2] = mxCreateNumericMatrix(1,c,mxUINT32_CLASS,mxREAL);
					uint32_t* seqptr = (uint32_t *)mxGetPr(plhs[2]);

					for(int n=0;n<c;n++)
						seqptr[n] =  *(((uint32_t *) buffer) + 4 + n);
				}
				else{
					plhs[0] = mxCreateDoubleScalar(-1.0f);
					plhs[1] = mxCreateDoubleScalar(0.0f);
					plhs[2] = mxCreateDoubleScalar(0.0f);
				}
			}
        break;
        case 't':
        	{
        		int clen = sprintf(commandbuf,"fcenter %d",args.centerfreq);
        		zmq_send(socketc,commandbuf,clen,0);
        	}
        break;
        case 'd':
        	{
        		
        		int clen = sprintf(commandbuf,"request rd");
        		//mexPrintf("disabling ref, sending %s, %d chars",commandbuf,clen);
        		zmq_send(socketc,commandbuf,clen,0);
        	}
        break;
        case 'e':
        	{
        		int clen = sprintf(commandbuf,"request re");
        		//mexPrintf("enabling ref, sending %s, %d chars",commandbuf,clen);
        		zmq_send(socketc,commandbuf,clen,0);
        	}
        break;
        case 's':
        	{
        		int clen = sprintf(commandbuf,"request sync");
        		zmq_send(socketc,commandbuf,clen,0);
        	}
        break;
        case 'c':
        	cleanup();
        break;
        default:
            break;

	}


}