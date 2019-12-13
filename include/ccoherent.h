#include <vector>
#include <atomic>
#include <thread>
#include <fftw3.h>

#include "csdrdevice.h"
#include "cpacketizer.h"
#include "common.h"
#include "cdsp.h"
#include "crefnoise.h"

using tmpcomplextype_ = std::complex<float>; //this can be done, but is it worth it?
using fftff_complex = tmpcomplextype_;


class ccoherent{
private:
	std::thread thread;
	static void threadf(ccoherent *);

	//std::vector<csdrdevice*> *devices; //CHANGED
	lvector<csdrdevice *> *devices;
	crefsdr *refdev;

	cpacketize *packetize;

	fft_scheme					fftscheme,ifftscheme;
	std::complex<float>			*sifft, *sfft;   //was fftwf_complex
	std::complex<float>			*sfloat, *sconv;
	float						*smagsqr;

	int 						nfft;
	int 						blocksize;

	int							mb; // RASPBERRYPI MAILBOX for gpu.

	std::vector<csdrdevice*>	lagqueue;
	crefnoise 					*refnoise;
public:

	std::atomic<bool> do_exit;
	//ccoherent(crefsdr*,std::vector<csdrdevice*> *, cpacketize *); //CHANGED
	//ccoherent(crefsdr*,lvector<csdrdevice*> *, cpacketize *);
	ccoherent(crefsdr*,lvector<csdrdevice*> *,crefnoise*, int nfft);
	~ccoherent();
	void start();
	void request_exit();
	void join();

	size_t lagqueuesize();

	void clearlagqueue();
	void queuelag(csdrdevice *d);
	void queuelag(csdrdevice *d,bool refc);
	void computelag();
};
