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

#include "ccoherent.h"

#include "unistd.h"
#include <iostream>
#include <mutex>

#ifdef RASPBERRYPI
int uint32log2(uint32_t k){
	int i=0;
	for (i=-1;k>=1;k>>=1,++i);
	return i;
}
#endif

ccoherent::ccoherent(crefsdr* refdev_,lvector<csdrdevice*> *devvec_,crefnoise* refnoise_, int nfft_){
	refdev =refdev_;
	devices=devvec_;
	refnoise=refnoise_;
	do_exit=false;
	nfft = nfft_;

	//TODO: get blocksize from refdev...!

	int alignment = volk_get_alignment();

	blocksize = refdev->get_blocksize();
	uint32_t bufferlen = blocksize*nfft;
	
	smagsqr	= (float *) volk_malloc(sizeof(float)*bufferlen,alignment);
	std::memset(smagsqr,0,sizeof(float)*bufferlen);

	// create plans for nfft transforms
#ifdef RASPBERRYPI
	int ret;
	int log2_N = uint32log2(blocksize);
	mb  = mbox_open();
	ret = gpu_fft_prepare(mb,log2_N,GPU_FFT_FWD,nfft,&fftscheme);
	if (ret){
		cout << "Failed to create "<< to_string(nfft) <<  "X gpu_fft_forward for log2N: " << to_string(log2_N) << endl;
	}
	ret = gpu_fft_prepare(mb,log2_N,GPU_FFT_REV,nfft,&ifftscheme); //perhaps check return values some day...
	if (ret){
		cout << "Failed to create "<< to_string(nfft) <<  "X gpu_fft_inverse for log2N: " << to_string(log2_N) << endl;
	}
	
#else

	//alloc buffers for dsp. note, fftwf_complex is bin. compat with lv_32fc_t and std::complex<float>.
	sfft  = (std::complex<float> *) fftwf_alloc_complex(bufferlen);
	sifft = (std::complex<float> *) fftwf_alloc_complex(bufferlen);
	sfloat 	= (lv_32fc_t *) volk_malloc(sizeof(lv_32fc_t)*bufferlen,alignment);
	sconv	= (lv_32fc_t *) volk_malloc(sizeof(lv_32fc_t)*bufferlen,alignment);

	//zero all buffers. important especially for circ. convolution.
	std::memset(sfft,0,sizeof(fftwf_complex)*bufferlen);
	std::memset(sifft,0,sizeof(fftwf_complex)*bufferlen);
	std::memset(sfloat,0,sizeof(lv_32fc_t)*bufferlen);
	std::memset(sconv,0,sizeof(lv_32fc_t)*bufferlen);

	
	int rank =1;
	int n[] = {blocksize};
	int *inembed = n; //apparently passing NULL is equivalent to passing n.
	int *onembed= n;
	int idist = blocksize;
	int odist = blocksize;
	int istride=1;
	int ostride=1;

	fftscheme  = fftwf_plan_many_dft(rank, n, nfft, (fftwf_complex *) sfloat,
								  inembed,istride,idist,(fftwf_complex *) sfft,
								  onembed,ostride,odist,FFTW_FORWARD,0);
	
	ifftscheme = fftwf_plan_many_dft(rank, n, nfft, (fftwf_complex *) sconv,
								  inembed,istride,idist,(fftwf_complex *) sifft,
								  onembed,ostride,odist,FFTW_BACKWARD,0);
#endif
}

ccoherent::~ccoherent(){
	if (thread.joinable()) thread.join();

	volk_free(smagsqr);
#ifdef RASPBERRYPI
	gpu_fft_release(fftscheme);
	gpu_fft_release(ifftscheme);
#else
	volk_free(sfloat);
	volk_free(sconv);
	fftwf_free(sfft);
	fftwf_free(sifft);
	fftwf_destroy_plan(fftscheme);
	fftwf_destroy_plan(ifftscheme);
#endif

}

void ccoherent::clearlagqueue(){
	lagqueue.clear();
}

size_t ccoherent::lagqueuesize(){
	return lagqueue.size();
}

void ccoherent::queuelag(csdrdevice *d){
	if (lagqueue.size()<nfft){
		/*printf("sfloatbase:%p\n",sfloat);
		printf("copy from: %p\n",d->get_sptr());
		printf("  copy to: %p\n",(sfloat+lagqueue.size()*blocksize));
		printf("nbytes   : %d\n",sizeof(std::complex<float>)*(blocksize));
		printf("blocksize: %d\n",(blocksize));*/

		//if (is_zeros((uint32_t*)sfloat,blocksize))
		//	cout << "ref is zeros" <<endl;
#ifdef RASPBERRYPI
		std::complex<float> *inptr = (std::complex<float> *) (fftscheme->in + lagqueue.size()*fftscheme->step);
		std::memcpy(inptr,d->get_sptr(),sizeof(std::complex<float>)*(blocksize));
#else
		std::memcpy((sfloat+lagqueue.size()*blocksize), d->get_sptr(),sizeof(std::complex<float>)*(blocksize)); //fix me, half of the copy is zeros.
#endif
		lagqueue.push_back(d);
		
	}
}
void ccoherent::queuelag(csdrdevice *d,bool refc){
	if (lagqueue.size()<nfft){
		//std::memcpy((sfloat+lagqueue.size()*blocksize), d->get_sptr(),sizeof(std::complex<float>)*(blocksize)); //fix me, half of the copy is zeros.
		if (refc)
			d->convtofloat(sfloat+lagqueue.size()*blocksize);
		else
			d->convtofloat(sfloat+lagqueue.size()*blocksize);
		lagqueue.push_back(d);
	}
}

void ccoherent::computelag()
{

#ifdef RASPBERRYPI
	
	cdsp::fft(&fftscheme);
	std::complex<float> *refptr = (std::complex<float> *) fftscheme->out;
	for (int k=1;k<lagqueue.size();k++){
		std::complex<float> *outptr = (std::complex<float> *) (fftscheme->out + k*fftscheme->step);
		std::complex<float> *inptr = (std::complex<float> *) (ifftscheme->in + k*ifftscheme->step);
		cdsp::conjugatemul(inptr,outptr,refptr,blocksize);
	}	
	cdsp::fft(&ifftscheme);

	for (int k=1;k<lagqueue.size();k++){
		std::complex<float> *outptr = (std::complex<float> *) (ifftscheme->out + k*ifftscheme->step);
		cdsp::magsquared(smagsqr+k*blocksize,outptr,blocksize);
	}
	//fft->in + j*fft->step;
#else
	cdsp::fft(sfft,sfloat,&fftscheme); //nfft fft:s

	//multiply each with ref
	for(int k=1;k<lagqueue.size();k++){
		cdsp::conjugatemul(sconv+k*blocksize,sfft+k*blocksize,sfft,blocksize);
	}

	//nfft inverse fft:s
	cdsp::fft(sifft,sconv,&ifftscheme);

	//squared magnitude for all except ref
	cdsp::magsquared(smagsqr+blocksize,sifft+blocksize,blocksize*(nfft-1));
	//cdsp::magsquared(smagsqr,sifft,blocksize*(nfft));
#endif

	for(int k=1;k<lagqueue.size();k++){
		float    lag=0.0f;
		float    D=0.0f,a=0.0f,b=0.0f;
		uint32_t idx = cdsp::indexofmax(smagsqr + k*blocksize,blocksize);
		//float    mag = smagsqr[k*blocksize+blocksize + idx];// *(smagsqr+k*blocksize+blocksize+lag);
		
		//edited
		//complex<float> corr = sifft[k*blocksize +idx];
		//float argh = std::arg(corr);
		//cout << to_string(corr.real()) <<"," << to_string(corr.imag()) << endl;
		//end edited

 //OLD CODE STARTS HERE
		
		float *ptr = (smagsqr + k*blocksize + idx);			// this should be taken from sifft() real part.
		float mag  = sqrt( *(ptr) / float(blocksize>>1) );  // not quite sure about the scaling. fftw is unnormalized, so ifft(fft(x)) = N*x
		
		if ((idx>1) && (idx<(blocksize-1))){ //check that n-1 and n+1 won't go off bounds
			a=-*(ptr-1)-2*(*ptr)+*(ptr+1);
			b=-0.5f*(*(ptr-1))+0.5f*(*(ptr+1));

			//handle possible divide by zero by setting the fractional to zero:
			if (a!=0.0f)
				D=-b/a;
			else{
				D=0.0f;
				cout << "frac zeroed at 1" <<endl;
			}
			
			lag = idx; //+D ,disabled fractional sync, obviously it's not doing what it's supposed to do.
			}
		else{
			lag = idx;
		}
/*
		complex<float> *ptrc = (sifft + (k+1)*blocksize/2 + idx);
		mag = (ptrc[0]*conj(ptrc[0])).real();
		a = -ptrc[-1].real() - 2.0f*ptrc[0].real() + ptrc[1].real();
		b = -0.5f*ptrc[-1].real() + 0.5f*ptrc[1].real();
		D = -(b/a)*(1.0f+M_PI/2);
		lag = D + idx;
*/

		lag-=(blocksize >> 1);
		lagqueue[k]->set_lag(lag,mag);
	}
	lagqueue.clear();

	//if (is_zeros((uint32_t*)sfloat,sizeof(std::complex<float>)*(blocksize>>1)))
	//	cout << "ref is zeros" <<endl;
}

void ccoherent::join(){
	thread.join();
}

void ccoherent::threadf(ccoherent *ctx){

	while(!ctx->do_exit){
		//read reference channel and perform fft:
		ctx->clearlagqueue();
		int8_t *refsptr = ctx->refdev->read();
		ctx->refdev->convtofloat(); //working
		ctx->queuelag(ctx->refdev);
		ctx->refdev->packetize.write(0,ctx->refdev->get_readcntbuf(),refsptr);
		ctx->refdev->consume();
		//ctx->queuelag(ctx->refdev,true);

		int c=1;
		{
			//std::unique_lock<std::mutex> lock(ctx->devices->m);
			//std::scoped_lock(ctx->devices->m);
			//ctx->devices->lock();
			for(auto *d: *ctx->devices){
				int8_t *ptr = d->read(); // this is the culprit for the hang...
				if(d->is_ready()){
					std::complex<float> *sfloat = (std::complex<float> *) d->convtofloat();
					if (d->is_lagrequested()){
						ctx->queuelag(d);
						//ctx->queuelag(d,false);
					}

					if (ctx->refnoise->isenabled()){
						std::complex<float> p = d->est_phasecorrect(ctx->refdev->get_sptr()+(ctx->refdev->get_blocksize()>>1));
					}
					
					d->phasecorrect();

					//d->packetize.write(c,d->get_readcnt(),ptr);
					d->packetize.write(c,d->get_readcntbuf(),sfloat);
					d->packetize.writedebug(c,d->get_phasecorrect());
					c++;
					d->consume();
				}
			}
			if (ctx->lagqueuesize()>1){
				ctx->computelag();
				//cout << "firing lag computation!" << endl;
			}
			ctx->refdev->packetize.notifysend();
			//ctx->devices->unlock();
		}
	}

	//ctx->packetize->request_exit();
}

void ccoherent::start(){
	thread = std::thread(&ccoherent::threadf,this);
}

void ccoherent::request_exit(){
	do_exit=true;
}
