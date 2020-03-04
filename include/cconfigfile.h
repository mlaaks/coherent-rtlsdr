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
#include <fstream>
#include <vector>

using namespace std;


struct sdrdefs {
	uint32_t devindex;
	string   serial;
};

class cconfigfile{
public:
	static std::vector<sdrdefs> readconfig(string fname) {
		string ln;
		ifstream cfgfile(fname);
		std::vector<sdrdefs> v;

		while(getline(cfgfile,ln)){
			sdrdefs d;
			if (ln[0]=='#')
				continue;
			else{
				std::size_t st,end;
				std::string::size_type sz;
				string ids = ln.substr(0,2);
				
				if ((ids[0]=='R')||(ids[1]=='R'))
					d.devindex = 0;
				else
					d.devindex = std::stoi(ids,&sz);
				
				st = ln.find(":");
				st = ln.find("'",st+1);
				end= ln.find("'",st+1);
				d.serial = ln.substr(st+1,end-st-1);
				v.push_back(d);
				}
			
		}
		cfgfile.close();
		return v;
	}

	static string get_refname(std::vector<sdrdefs> vdefs){
		string ret;
		for(auto n : vdefs) {
				if (n.devindex==0){
					ret=n.serial;
					break;
				}
			}
		return ret;
	}

};
