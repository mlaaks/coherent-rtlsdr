#ifndef CREFNOISEH
#define CREFNOISEH

#include <iostream>
#include <fstream>

class crefnoise{
private:
	ofstream fp;
	bool enabled;
	
public:
	void set_state(bool s){
		enabled = s;
		if (s)
			fp<<"x"; //enable char
		else
			fp<<"o"; //disable char (right now any other than 'x')
		
		fp.flush(); //may not send single char immediately unless we flush buffers
	};

	bool isenabled(){
		return enabled;
	}

	crefnoise(std::string dev){
		fp.open(dev.data());
		if (!fp.good())
			cout<<"Error opening reference noise control '" << dev << endl;
		else{
			enabled = true;
			fp<<"x";
			fp.flush();
		}

	};
	~crefnoise(){
		fp.close();
	};
};
#endif
