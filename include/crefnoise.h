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
