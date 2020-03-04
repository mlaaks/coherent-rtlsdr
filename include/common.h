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

#ifndef COMMONH
#define COMMONH

#include <condition_variable>
#include <mutex>
#include "cdsp.h"

#include <vector>
#include <iterator>
#include <cstring> //memset
#include <queue>

#define USELIBRTLSDRBUFS

const float sync_threshold=0.005;

#define TWO_POW(n)		((double)(1ULL<<(n)))

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

//a circular pointer array:
class cbuffer{
public:
    int8_t      **ptr;
    uint32_t    *readcnt;
    uint64_t    *timestamp;
    uint32_t    L;
    uint32_t	N;

    uint32_t    wp;
    uint32_t    rp;

    cbuffer(uint32_t n, uint32_t l){
    	ptr 	  = new int8_t* [n];
    	readcnt   = new uint32_t [n];
    	timestamp = new uint64_t [n];



        wp = 0;
        rp = 0;
    	N=n;
    	L=l;

    	int alignment = volk_get_alignment();
    	for (int i=0;i<N;++i){
            readcnt[i]=0;
            timestamp[i]=0;

#ifndef USELIBRTLSDRBUFS
			ptr[i] = (int8_t *) volk_malloc(sizeof(int8_t)*L,alignment);
			std::memset(ptr[i],0,sizeof(int8_t)*L);
#else
			ptr[i] = NULL;
#endif		
		}
    };

    ~cbuffer(){
#ifndef USELIBRTLSDRBUFS
    	for (int i=0;i<N;++i)
			volk_free(ptr[i]);
#endif
    	delete[] ptr;
        delete[] readcnt;
        delete[] timestamp;
    };
/*
    void setbufferptr(uint8_t *buffer,uint32_t rcnt)
	{
		timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
#ifdef USELIBRTLSDRBUFS
		ptr[rcnt % N] = (int8_t *) buffer;
#endif
		cdsp::convtosigned(buffer, (uint8_t *) ptr[rcnt % N],L);
		readcnt=rcnt;
	};

	int8_t *getbufferptr(){
#ifndef USELIBRTLSDRBUFS		
		return ptr[(readcnt-1) % N];
#else
		//we might return a null pointer during the first run through the circular pointer array. Hack to avoid that...
		return ptr[(readcnt-1) % N] != NULL ? ptr[(readcnt -1) % N] : ptr[readcnt % N]; 
#endif
	}
	int8_t *getbufferptr(uint32_t rcnt){
#ifndef USELIBRTLSDRBUFS		
		return ptr[rcnt % N];
#else
		return ptr[rcnt % N] != NULL ? ptr[rcnt % N] : ptr[(readcnt) % N];
#endif
	}
*/
void setbufferptr(uint8_t *buffer,uint32_t rcnt)
{
    timestamp[((wp) & (N-1))] = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    readcnt[((wp) & (N-1))] = rcnt;
#ifdef USELIBRTLSDRBUFS
    ptr[((wp) & (N-1))] = (int8_t *) buffer;
#endif
    cdsp::convtosigned(buffer, (uint8_t *) ptr[wp++ & (N-1)],L);
};

int8_t *getbufferptr(){
#ifndef USELIBRTLSDRBUFS        
        return ptr[rp & (N-1)];
#else
        //we might return a null pointer during the first run through the circular pointer array. Hack to avoid that...
        return ptr[rp & (N-1)] != NULL ? ptr[rp & (N-1)] : ptr[0]; 
#endif
    }

uint32_t get_rcnt(){
    return readcnt[rp & (N-1)];
}

void consume(){
    rp++;
}


int8_t *getbufferptr(uint32_t rcnt){
#ifndef USELIBRTLSDRBUFS        
        return ptr[rcnt % N];
#else
        return ptr[rcnt % N] != NULL ? ptr[rcnt % N] : ptr[0];
#endif
}
};

class barrier
{
private:
    std::mutex mtx;
    std::condition_variable cv;
    std::size_t cnt;
public:
    explicit barrier(std::size_t cnt) : cnt{cnt} { }
    void wait()
    {
        std::unique_lock<std::mutex> lock{mtx};
        if (--cnt == 0) {
            cv.notify_all();
        } else {
            cv.wait(lock, [this] { return cnt == 0; });
        }
    }
};

/*template <class InputIterator, class Allocator = allocator<typename iterator_traits<InputIterator>::value_type>>
   vector(InputIterator, InputIterator, Allocator = Allocator())
   -> vector<typename iterator_traits<InputIterator>::value_type, Allocator>;
template <class Allocator> struct hash<std::vector<bool, Allocator>>;
*/


//template <class T, class Allocator> bool operator!=(const vector<T,Allocator>& x, const vector<T,Allocator>& y

//template <class T>class lvector;

using namespace std;
template <class T>
class lvector{
private:
//    using vector_t = std::vector<T>;
    std::vector<T> q;
    //T a;
    //std::condition_variable cv;
public:
	mutable std::mutex m;
	//std::mutex m;
    explicit lvector(): q(){
    }
    lvector(T obj): q(obj){
    }

    void push_back(const T& v){
    	std::lock_guard<std::mutex> lock(m);
        q.push_back(v);
    };
    void pop_back(){
    	std::lock_guard<std::mutex> lock(m);
        q.pop_back();
    };



    //typedef std::iterator<std::vector<T>::iterator> iterator;
    using iterator = typename std::vector<T>::iterator;
  //  using iterator = vector_t::iterator;
   // using const_iterator = vector_t::const_iterator;
    //std::iterator<T,std::allocator<T>> begin(){
   //std::iterator<T, std::allocator<T> > begin(){

    void lock(){
    	m.lock();
    };
    void unlock(){
        m.unlock();
    };
    void erase(iterator it){
      	std::lock_guard<std::mutex> lock(m);
        q.erase(it);
    }

    const iterator begin(){
        return q.begin();
    }


    const iterator end(){
        return q.end();
    }
    T& back(){
        return q.back();
    }
    

    size_t size() const{
        return q.size();
    };

          T& operator[](std::size_t idx)       { /*std::unique_lock<std::mutex> lock(m); */ return q[idx]; }
    const T& operator[](std::size_t idx) const { /*std::unique_lock<std::mutex> lock(m); */ return q[idx]; }
    bool operator!=(const T& a){ return !(a == q);}
};


template <class T>
class lqueue{
private:
    std::queue<T> q;
    std::condition_variable cv;
    std::mutex m;
public:
    lqueue(): q(){
    }
    lqueue(T obj): q(obj){
    }

    void push(const T & v){
        std::lock_guard<std::mutex> lock(m);
        q.push(v);
        cv.notify_one();
    };

    T& front(){
       // std::unique_lock<std::mutex> lock(m);
       // cv.wait(lock, [this]{return (q.size() > 0);});
        return q.front();
    };
    T pop(){
        {
            std::unique_lock<std::mutex> lock(m);
            cv.wait(lock, [this]{return (q.size()>0);});
        }
        T v =q.front(); // was T& v, reference got bad after certain number of chars when T=std::string

        //std::cout << "we have: "<< v << std::endl;
        q.pop();
        return v;
    };

    size_t size() const{
        return q.size();
    };
};

bool is_zeros(uint32_t *p,int n);

#endif