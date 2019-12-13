#include "ccontrol.h"
#include "common.h"
#include <unistd.h> //usleep remove...
#include <errno.h>
#include <pthread.h>
#include <signal.h>

const uint32_t rtl_xtal = 28800000;

const double maxppm = TWO_POW(13)/TWO_POW(24); //TWO_POW(-11)
const float  scale = 100.0f; //WAS 75. This works well for 1e6 sample rate
const float  frac_t = 0.90f;

void hndlr(int in){

}

double realfs(uint32_t requestedfs){
//from rtlsdr source
	//#define APPLY_PPM_CORR(val,ppm) (((val) * (1.0 + (ppm) / 1e6)))

	uint32_t fsratio = (uint32_t)((rtl_xtal * TWO_POW(22)) / requestedfs);
	fsratio &= 0x0ffffffc;

	uint32_t real_fsratio = fsratio | ((fsratio & 0x08000000) << 1);
	return (rtl_xtal * TWO_POW(22)) / real_fsratio;
}

void ccontrol::start(){
	do_exit= false;

	thread = std::thread(&ccontrol::threadf, this);
}

void ccontrol::request_exit(){
	do_exit=true;
}

void ccontrol::join(){
	thread.join();
}

void fillts(struct timespec* t,double ts){
	long int tnsec = ts*1e9;
	if (ts>1.0){
		t->tv_sec = floor(ts);
		t->tv_nsec= (ts - floor(ts))*1e9;
	}
	else
	{
		t->tv_sec=0;
		t->tv_nsec = tnsec; 
	}
}

float descent(float lag){
	//return fabs(maxppm*tanh(lag/scale));
	return maxppm*tanh(lag/scale); //EDIT
}

void ccontrol::threadf(ccontrol *ctx){
	
	struct timespec tsp = {1,0L};
	long int        tns = 0L;
	time_t	 		 ts = 0;

	//std::cout << "entering control thread with do exit " << std::to_string(ctx->do_exit) << std::endl;
	usleep(16*32800); //UGLY! //this should be resolved: if this wait is removed, segfault happens...
	//ctx->dev->wait_synchronized(); // check if wait before the loop resolves...nope.

	while(!ctx->do_exit){
		if(!ctx->dev->wait_synchronized()){

			ctx->dev->requestfftblocking(); //ask for a new fft.
			
			float    lag = ctx->dev->get_lagp()->lag;
			uint32_t   L = ctx->dev->get_blocksize();
			float	  fs = realfs(ctx->dev->get_samplerate());
			float fcorr;

			//std::cout << "lag is" << std::to_string(lag) << std::endl;
			if (fabs(lag)>sync_threshold){ //should we check when the estimate is calculated ?
				
				float      p = descent(lag);
				double	   t = frac_t*fabs(lag/(p*fs)); //time to spend at altered samplerate
				
				fillts(&tsp,t);
				
				//set the resampler frequency:	
				//fcorr = (float) sgn(lag)*p;
				fcorr = p; //EDIT 

				ctx->dev->set_correction_f(fcorr);
				
				nanosleep(&tsp,NULL); //the second argument is a timepec *rem (aining)
				//if (errno==EINTR)
				//	cout << "we've been interrupted" << endl;
				ctx->dev->set_correction_f(0.0f);
			}
			else{
				ctx->dev->set_correction_f(0.0f);
				ctx->dev->set_synchronized(true);
			}
		}
	}
}


