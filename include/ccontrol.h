#ifndef CCONTROLH
#define CCONTROLH

#include <atomic>
#include <thread>
#include "csdrdevice.h"

//forward declaration, these classes have a circular dependency
class ccontrol;
class csdrdevice;

class ccontrol{
private:
	std::thread thread;
	static void threadf(ccontrol *);
	
	csdrdevice *dev;
public:
	std::atomic<bool> do_exit;

	void start();
	void request_exit();
	void join();
	ccontrol(csdrdevice *device){
		dev=device;
		do_exit = false;	
	}

	~ccontrol(){
		if (thread.joinable()) thread.join();
	}

};

#endif

