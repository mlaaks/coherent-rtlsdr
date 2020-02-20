#include "csdrdevice.h"

csdrdevice::csdrdevice(uint32_t asyncbufn_, uint32_t blocksize_, uint32_t samplerate_, uint32_t fcenter_) : thread(){
	asyncbufn = asyncbufn_;
	blocksize = blocksize_;
	samplerate= samplerate_;
	fcenter   = fcenter_;

	realfs 	= 0;
	readcnt = 0;
	do_exit = false;
	lagrequested = true;
	lagready =false;
	synced  = false;
	newdata = 0; //false;
	streaming=false;
	devname = "";

	lagp.lag = 0;
	lagp.ts  = 0;

	phasecorr     = std::complex<float>(1.0f,0.0f);
	phasecorrprev = std::complex<float>(1.0f,0.0f);

	int alignment = volk_get_alignment();
	sfloat 	= (lv_32fc_t *) volk_malloc(sizeof(lv_32fc_t)*blocksize,alignment);

	std::memset(sfloat,0,sizeof(lv_32fc_t)*blocksize);

	controller = new ccontrol(this);
}

csdrdevice::~csdrdevice(){
	if (thread.joinable()) thread.join();

	volk_free(sfloat);
	
	delete controller;
}

std::complex<float> csdrdevice::est_phasecorrect(const lv_32fc_t *ref){
	
	float alpha = 0.5f;
	std::complex<float> correlation=0.0f;
	volk_32fc_x2_conjugate_dot_prod_32fc((lv_32fc_t *) &correlation,sfloat,ref,(blocksize >> 1));
	phasecorr  = std::conj(correlation) * (1.0f /std::abs(correlation));


	phasecorr = alpha*phasecorr + (1-alpha)*phasecorrprev;
	phasecorrprev = phasecorr;
	return phasecorr;
}

float csdrdevice::est_PAPR(const lv_32fc_t *ref){


}

std::complex<float> csdrdevice::get_phasecorrect(){
	return phasecorr;
}

std::complex<float> *csdrdevice::phasecorrect(){
	
	cdsp::scalarmul(sfloat,sfloat,phasecorr,(blocksize>>1));
	return sfloat;
}

//float est_phasecorrect(const lv_32fc_t *ref);
//	float get_phasecorrect();
