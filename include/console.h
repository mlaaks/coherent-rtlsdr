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

#include <iostream>
#include <sstream>
#include <csignal>
#include <atomic>
#include <map>
#include <unordered_map>
#include <thread>
#include <signal.h>
#include <vector>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include <zmq.hpp>
#include "csdrdevice.h"
#include "common.h"

#include "crefnoise.h"


using namespace std;

enum command_code{
	help,
	fs,
	add,
	del,
	status,
	list,
	nop,
	logs,
	quit,
	fcenter,
	request,
	phase
};

//static std::map<std::string, command_code> command_codes;

const static std::unordered_map<std::string,int> command_codes{
	{"help",help},
	{"fs",fs},
	{"add",add},
	{"del",del},
	{"status",status},
	{"list",list},
	{"nop",nop},
	{"log",logs},
	{"quit",quit},
	{"fcenter",fcenter},
	{"request",request},
	{"phase",phase}
};

struct phistory_t{
	std::chrono::high_resolution_clock::time_point t;
	std::complex<float> p;
	//auto t = std::chrono::high_resolution_clock::now();
	//auto t_ns= (std::chrono::time_point_cast<std::chrono::nanoseconds>(t)).time_since_epoch();
};


class cconsole{
private:
	barrier *startbarrier;
	int *stderr_pipe;
	crefsdr *refdev;
	std::thread thread;
	static void consolethreadf(cconsole *);

	std::vector<phistory_t> phistory;

	crefnoise *refnoise;

	//zmq::context_t 		context;
	//zmq::socket_t  		socket;

public:
	//std::vector<csdrdevice*> *devices; //CHANGED
	lvector<csdrdevice*> *devices;

	std::atomic<bool> do_exit;
	//cconsole(int p[2],csdrdevice *refdev_, std::vector<csdrdevice*> *devvec_); // CHANGED
	cconsole(int p[2],crefsdr *refdev_, lvector<csdrdevice*> *devvec_,crefnoise * refnoise_);
	~cconsole();
	void start();

	void request_exit();
	void join();

	int parsecmd(std::string);
	string getoptionstr(std::string inputln);

	int cmdlist(std::string);
	int cmdretune(std::string);
	int cmdadd(std::string);
	int cmddel(std::string);
	int cmdrequest(std::string);
	int cmdfs(std::string);
	int cmdphase(std::string);
	int cmdstatus(std::string);
};