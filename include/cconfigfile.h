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
