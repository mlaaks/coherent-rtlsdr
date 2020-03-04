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

