#ifndef CSDRDEVICEH
#define CSDRDEVICEH

#include <rtl-sdr.h>
#include <chrono>
#include <iostream>
#include <cstring>
#include <atomic>
#include <thread>

#include <volk/volk.h>
#include <fftw3.h>
#include <complex>

#include "ccontrol.h"
#include "common.h"
#include "cdsp.h"
#include "cpacketizer.h"


//forward declaration, these classes have a circular dependency
class csdrdevice;
class ccontrol;

struct lagpoint{
	uint64_t ts;
	float 	 lag;
	float 	 mag;
    float    PAPR;
	lagpoint()
	{
		ts=0;
		lag=0;
		mag=0;
        PAPR=0;
	}
};

class csdrdevice{
	uint32_t					readcnt;

protected:
	ccontrol 					*controller;
	std::thread 				thread;
	std::mutex 					mtx;
	std::condition_variable 	cv;

	std::mutex					fftmtx;
	std::condition_variable		fftcv;

	std::mutex					syncmtx;
	std::condition_variable 	synccv;

	std::atomic<bool> 			newdata;
	std::atomic<bool> 			do_exit;

	std::atomic<bool>			lagrequested;
	std::atomic<bool>			lagready;
	std::atomic<bool> 			synced;
	std::atomic<bool>			streaming;

	std::complex<float>			*sfloat; //was lv_32fc_t *

	std::complex<float>			phasecorr;
	std::complex<float>	 		phasecorrprev;

	uint32_t					asyncbufn; //should this still be under crtlsdr...
	uint32_t					blocksize;
	uint32_t	 				samplerate;
	uint32_t 					realfs;
	uint32_t					fcenter;
	lagpoint 					lagp;

	
	std::string					devname;
	
	//virtual void asynch_thread(csdrdevice *d) = 0;
public:
	cpacketize					packetize;
	
	virtual int open(uint32_t index) = 0;//(uint32_t ndevice,uint32_t fcenter, uint32_t samplerate,uint32_t gain,uint32_t agcmode)=0;
	virtual int open(std::string name) = 0; 
	virtual int close() = 0;
	virtual int set_fcenter(uint32_t f) = 0;
	virtual int set_samplerate(uint32_t fs) = 0;

	virtual int set_agcmode(bool flag)=0;
	virtual int set_tunergain(uint32_t gain)=0;
	virtual int set_tunergainmode(uint32_t mode)=0;
	virtual int set_correction_f(float f)=0;

	virtual int8_t *swapbuffer(uint8_t *b) = 0;
	virtual int8_t *read()=0;

	virtual void start(barrier *b) = 0;
	virtual void startcontrol() = 0;
	virtual void stop(){};
	virtual const std::complex<float> *convtofloat() = 0;
	virtual const std::complex<float> *convtofloat(const std::complex<float>*)=0;
	virtual void consume() = 0;
    virtual uint32_t get_readcntbuf()=0;
	//virtual float findcorrpeak(const fftwf_complex *reffft) = 0;

	std::complex<float> est_phasecorrect(const lv_32fc_t *ref);
	std::complex<float> get_phasecorrect();
    
    float est_PAPR(const lv_32fc_t *ref);

	std::complex<float> *phasecorrect();

	void 	requestfft(){lagready=false; lagrequested=true;};
	void	requestfftblocking(){
				lagrequested=true; 
				lagready=false; 
				{
				 std::unique_lock<std::mutex> lock(fftmtx);
				 fftcv.wait(lock,[this]{return lagready.load();});
				}
			};

	void	set_lag(float lag,float mag){
		lagp.lag = lag;
		lagp.mag = mag;
		lagp.ts  = (std::chrono::time_point_cast<std::chrono::nanoseconds>
			       (std::chrono::high_resolution_clock::now())).time_since_epoch().count();

		{
        	std::lock_guard<std::mutex> lock(fftmtx);
        	lagready = true;
        	lagrequested = false;
        }

        fftcv.notify_all();
	}

	bool	is_lagrequested(){
		return lagrequested;
	}

	const std::complex<float>* get_sptr(){return sfloat;};

	void request_exit(){do_exit=true;};
	
	const lagpoint* const get_lagp(){return &lagp;}; //??
	float 	 get_samplerate(){return samplerate;}; //should we return the 'actual samplerate' reported by set_samplerate()?
	uint32_t get_blocksize(){return blocksize;};
	uint32_t get_asyncbufn(){return asyncbufn;};
	uint32_t get_fcenter(){return fcenter;};

	std::string get_devname(){return devname;};


	inline uint32_t inc_readcnt(){return readcnt++;};
	inline uint32_t get_readcnt(){return readcnt;};
	
	inline bool wait_synchronized(){
		//return synced;
		{
			std::unique_lock<std::mutex> lock(syncmtx);
			synccv.wait(lock,[this]{return !synced.load();});
		}
		return synced;
	};
	inline void set_synchronized(bool state){
		std::lock_guard<std::mutex> lock(syncmtx);
		synced = state;
		synccv.notify_one();
	};

	inline bool is_ready(){
		return ((readcnt >= asyncbufn) && streaming);
	};

	inline bool get_synchronized(){return synced;};

	csdrdevice(uint32_t asyncbufn_,uint32_t blocksize_,uint32_t samplerate_, uint32_t fcenter_);
	virtual ~csdrdevice();
};


class crtlsdr: public csdrdevice{
	rtlsdr_dev_t	*dev;
	uint32_t	 	devnum;			//rtlsdr device params
	
	uint32_t		rfgain;
	bool			enableagc;
	
	static void 	asynch_callback(unsigned char *buf, uint32_t len, void *ctx);

	

	int8_t	 		*swapbuffer(uint8_t *b);
protected:
    cbuffer         s8bit;  //timestamped & seqnum, N 8-bit buffers
	static void 	asynch_threadf(crtlsdr *d);
	barrier			*startbarrier;
	
public:
	static uint32_t get_device_count();

	static std::string get_device_name(uint32_t index);
	static int         get_index_by_serial(std::string serial);
	static std::string get_device_serial(uint32_t index);
	static std::string get_usb_str_concat(uint32_t index);

	int open(uint32_t index);
	int open(std::string name);

	int close();
	int set_fcenter(uint32_t f);
	int set_samplerate(uint32_t fs);

	int set_agcmode(bool flag);
	int set_tunergain(uint32_t gain);
	int set_tunergainmode(uint32_t mode);
	int set_correction_f(float f);

	const std::complex<float> *convtofloat(); //needed for overloading in case of reference channel.
	const std::complex<float> *convtofloat(const std::complex<float>*);
	//float findcorrpeak(const fftwf_complex *reffft);

	//float est_phasecorrect(const lv_32fc_t *ref);


	void start(barrier *b);
	void startcontrol();
	void stop();
    
    uint32_t get_readcntbuf(){
        return s8bit.get_rcnt();
    };
    
	int8_t *read();
	void consume(){s8bit.consume();};

	crtlsdr(uint32_t asyncbufn_, uint32_t blocksize_,uint32_t samplerate_, uint32_t fcenter_) : csdrdevice(asyncbufn_,blocksize_,samplerate_,fcenter_), s8bit(asyncbufn_,blocksize_){
		rfgain   = 500;
		//int alignment = volk_get_alignment();

		//s8bit = new cbuffer[asyncbufn]; //{cbuffer(block_size)}
		//for (int n=0;n<asyncbufn;++n) s8bit[n].L=block_size; //could not find a way to work the initializer lists above...

		//s8bit.ptr = (int8_t *) volk_malloc(sizeof(int8_t)*blocksize,alignment);
		//s8bit.N=blocksize; s8bit.timestamp=0; s8bit.readcnt=0;
	};
	~crtlsdr(){
		//delete [] s8bit;
		//volk_free(s8bit.ptr);
	};
};


class czmqsdr: public csdrdevice{

};

class crefsdr: public crtlsdr{
	//needed for overloading in case of reference channel. this just places the data in the second half of the buffer.
	
public:
	//const fftwf_complex* fft();
	const std::complex<float> *convtofloat();
	const std::complex<float> *convtofloat(const std::complex<float>*);

	const lv_32fc_t* get_sptr();

	void start(barrier *b);
	crefsdr(uint32_t asyncbufn_, uint32_t blocksize_,uint32_t samplerate_, uint32_t fcenter_) : crtlsdr(asyncbufn_,blocksize_,samplerate_,fcenter_){};
	//~crefsdr(){};
};

#endif
