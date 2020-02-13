//builds with:
// g++ main.cc ccoherent.cc crtlsdr.cc cdsp.cc console.cc -lm -lpthread -lrtlsdr -lvolk -lfftw3f

#include <iostream>
#include <sstream>
#include <csignal>
#include <atomic>
#include <thread>
#include <vector>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include "console.h"
#include "csdrdevice.h"
#include "ccoherent.h"
#include "cpacketizer.h"
#include "common.h"
#include "crefnoise.h"
#include "cconfigfile.h"

using namespace std;
int stderr_pipe[2];		//for redirecting stderr, where librtlsdr outputs
FILE *stderr_f;

std::atomic<bool> exit_all;
//std::sig_atomic_t sig;
//barrier *bar;

void signal_handler(int signal)
{
  if (signal==SIGINT){
  	//cout << "caught SIGINT" << std::to_string(signal) << endl;
  	cout << endl << "caught CTRL+C (SIGINT), exiting after CTRL+D..." <<endl; //std::to_string(signal)
  	exit_all=true;
  }
}
void int_handler(int signal)
{
	if (signal==SIGUSR1){
		//dummy handler, for waking up sleeping control threads...
	}
}

//redirect C fprintf(stder,...) to a pipe, suppress stdout output from librtlsdr
void redir_stderr(bool topipe){
	pipe(stderr_pipe);	
	stderr_f = fdopen(stderr_pipe[0],"r");
	dup2(stderr_pipe[1],2);
	close(stderr_pipe[1]);
	fcntl(stderr_pipe[0], F_SETFL, O_NONBLOCK);
	//freopen ("stderrlog","w",stderr); //redirect to a file...
}

struct cl_ops{
	string   refname;
	bool     agc;
	uint32_t fs;
	uint32_t fc;
	uint32_t asyncbufn;
	uint32_t blocksize;
	int		 ndev;
	uint32_t gain;
	uint32_t refgain;
	bool	 no_header;
	string 	 config_fname;
	bool	 use_cfg;
	bool	 quiet;
};

void usage(void)
{
	fprintf(stderr,
		"\ncoherentsdr - synchronous rtl-sdr reader, IQ-samples published to a ZMQ-socket (atm, this is tcp://*:5555)\n"
		"Dongles need to be clocked from the same signal, otherwise cross-correlation and synchronization will result in garbage\n\n"
		"Usage:\n"
		"\t[-f frequency_to_tune_to [Hz'(default 480MHz)]\n"
		"\t[-b blocksize [samples'(default 2^14=16384, use 2^n)]\n"
		"\t[-s samplerate (default: 2048000 Hz)]\n"
		"\t[-n number of devices to init\n"
		"\t[-S use synchronous read (default: async) NOT FUNCTIONAL CURRENTLY]\n"
		"\t[-g tuner gain: signal [dB] (default 60)]\n"
		"\t[-r tuner gain: reference [dB] (default 50)]\n"
		"\t[-I reference dongle serial ID (default 'MREF')]\n"
		"\t[-A set automatic gaincontrol for all devices]\n"
		"\t[-C 'config file', read receiver config from a file]\n"
		"\t[-q quiet mode, redirect stderr from rtl-sdr\n"
		"\t[-R outputmode raw: no packet header.]\n");
	exit(1);
}

int parsecommandline(cl_ops *ops, int argc, char **argv){
	int opt;
	while ((opt = getopt(argc, argv, "s:b:f:h:n:g:r:I:C:ARq")) != -1) {
		switch (opt) {
			case 's':
					ops->fs=(uint32_t)atof(optarg);
				break;
			case 'b':
					ops->blocksize = (uint32_t)atoi(optarg);
				break;
			case 'f':
					ops->fc=(uint32_t) atof(optarg);
				break;
			case 'h':
					usage();
				break;
			case 'n':
					if ((uint32_t)atoi(optarg)<=ops->ndev)
						ops->ndev =(uint32_t)atoi(optarg);
					else
						cout << "Requested device count higher than devices connected to system " 
						<< to_string(ops->ndev) << ", setting ndev=" << to_string(ops->ndev) << endl;  
				break;
			case 'g':
					ops->gain = (uint32_t) atoi(optarg)*10;
				break;
			case 'r':
					ops->refgain = (uint32_t) atoi(optarg)*10;
				break;
			case 'I':
					ops->refname=std::string(optarg);
				break;
			case 'C':
				ops->config_fname = std::string(optarg);
				ops->use_cfg = true;
			break;
			case 'A':
					ops->agc = true;
				break;
			case 'R':
					ops->no_header = true;
				break;
			case 'q':
					ops->quiet = true;
				break;
			default:
				usage();
				break;
		}
	}
	return 0;
};

int main(int argc, char **argv)
{

	int nfft = 8;

	cl_ops   ops = {"M REF",false,2048000,uint32_t(1024e6),8,1<<14,4,500,500,false,"",false,false};
	ops.ndev = crtlsdr::get_device_count();
	cout << to_string(ops.ndev) << " devices found." << endl;
	parsecommandline(&ops,argc,argv);

	cout << "ops parsed\n"<< endl;
	if (ops.no_header)
		cout << "streaming in raw mode" << endl;

	if (crtlsdr::get_index_by_serial(ops.refname)<0){
		cout << "reference '"<< ops.refname <<"' not found, exiting" << endl;
		exit(1);
	}

	{
		barrier *startbarrier;
		crefnoise * refnoise = new crefnoise("/dev/ttyACM0");

		//redirect stderr stream
		std::stringstream buffer;
		std::streambuf *old;

		exit_all=false;	

		if (ops.quiet){
			old = std::cerr.rdbuf(buffer.rdbuf()); 
			redir_stderr(true);
		}

		std::signal(SIGINT,signal_handler);
		std::signal(SIGUSR1,int_handler);

		//std::vector<csdrdevice *> v_devices; //CHANGED
		lvector<csdrdevice *> v_devices;
		
		//read device order from config file, if not given, build order by device index:
		std::vector<sdrdefs> vdefs;
		if (ops.use_cfg){
			cout << "Reading config " << ops.config_fname << endl;
			vdefs = cconfigfile::readconfig(ops.config_fname);
			ops.ndev = vdefs.size();
			ops.refname = cconfigfile::get_refname(vdefs);
		}
		else{
			uint32_t refindex = crtlsdr::get_index_by_serial(ops.refname);
			for(uint32_t n=0;n<ops.ndev;n++){
				sdrdefs d;
				d.devindex=n;
				d.serial  = crtlsdr::get_device_serial(n);
				if (n!=refindex)
					vdefs.push_back(d);
			}

		}

		crefsdr* ref_dev = new crefsdr(ops.asyncbufn,ops.blocksize,ops.fs,ops.fc);
		cout << "opening reference device" <<endl;

		if (ref_dev->open(ops.refname)){
			cout <<"could not open reference, serial number:'" << ops.refname <<"'"<<endl;
			exit(1);
		}

		cout << "opening signal devices:";

		for (auto n: vdefs){
			v_devices.push_back(new crtlsdr(ops.asyncbufn,ops.blocksize,ops.fs,ops.fc)); //this must be made std::unique_ptr or std::shared_ptr...
			if (v_devices.back()->open(n.serial)){
				delete v_devices.back();
				v_devices.pop_back();
				cout<<"!";
			}
			else
			{
				cout<<"*";
				v_devices.back()->set_agcmode(ops.agc);
			}
			cout.flush();
			/*if (exit_all){
				break;
			}*/
		}
		cout << endl;
		
		cout << "creating barrier" <<endl;
		startbarrier = new barrier(v_devices.size()+1);
		
		cout << "starting capture on the reference device" <<endl;
		ref_dev->start(startbarrier);
		cout << "starting capture on the signal devices" <<endl;
		for (auto d : v_devices)
			d->start(startbarrier);
		
		//cpacketize* packetize = new cpacketize(v_devices.size(),ops.blocksize, "tcp://*:5555", false);
		cpacketize::init("tcp://*:5555",ops.no_header,v_devices.size()+1,ops.blocksize);

		cconsole console(stderr_pipe,ref_dev, &v_devices,refnoise);
		
		console.start();

		//sleep(1);
		
		cout << "starting coherence" <<endl;
		

		ccoherent coherent(ref_dev,&v_devices, refnoise, nfft);
		coherent.start();
		
		cout << "entering main loop" <<endl;
		
		while(!exit_all){
			cpacketize::send(); //main thread just waits on data and publishes when available.
		}

		cout << "stopping coherent thrd" <<endl;
		coherent.request_exit(); coherent.join();

		cout << "stopping devices" <<endl;

		ref_dev->stop(); ref_dev->request_exit(); ref_dev->close();

		for (auto d : v_devices){
			d->stop();
			d->request_exit();
		}

		cout << "stopping console" <<endl;
		console.request_exit();

		//console.join();
		//v[0].close();
		cout << "closing devices" <<endl;
		for (auto d : v_devices)
			d->close();

		console.join();

		if (ops.quiet){
			std::cerr.rdbuf( old );
			close(stderr_pipe[0]);
		}
		
		delete startbarrier;
		cout << "closing reference" <<endl;
		delete ref_dev;
		//delete packetize;
		cpacketize::cleanup();
		delete refnoise;
	}
	return 1;
}