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

#include "csdrdevice.h"
#include <unistd.h>
#include <csignal>
//extern barrier* bar;
//barrier *crtlsdr::startbarrier = NULL;

void crtlsdr::start(barrier *b){
	startbarrier = b;
	streaming = true;
	thread = std::thread(crtlsdr::asynch_threadf,this);
	controller->start();
	
}

void crtlsdr::startcontrol(){
	//controller->start();
}

void crtlsdr::stop(){
	rtlsdr_cancel_async(dev);
	streaming = false;
	cv.notify_all();
	controller->request_exit();
	std::raise(SIGUSR1);
}

void crtlsdr::asynch_threadf(crtlsdr *d){ //a static func
	int ret;
	//std::cout << "starting #" << std::to_string(d->devnum) << std::endl;
	
	d->startbarrier->wait();
	
	ret = rtlsdr_reset_buffer(d->dev);
	if (ret<0)
		std::cerr << "rtlsdr_reset_buffer failed for#" << std::to_string(d->devnum) << std::endl;

	ret = rtlsdr_read_async(d->dev,crtlsdr::asynch_callback, (void *)d, d->asyncbufn, d->blocksize);
	if (ret<0)
		std::cerr << "rtlsdr_read_async failed for #" << std::to_string(d->devnum) << std::endl;
	else
		std::cerr << "asynch_thread #" << std::to_string(d->devnum) << "exited" << std::endl;
}

void crtlsdr::asynch_callback(unsigned char *buf, uint32_t len, void *ctx)
{
	if (ctx) {
		((csdrdevice *)ctx) -> swapbuffer(buf);
		//csdrdevice *d = (csdrdevice *)ctx;
		//d->swapbuffer(buf);
	}
}

uint32_t crtlsdr::get_device_count(){
	return rtlsdr_get_device_count();
}

std::string crtlsdr::get_device_name(uint32_t index){
	return rtlsdr_get_device_name(index);
}

int crtlsdr::get_index_by_serial(std::string serial){
	return rtlsdr_get_index_by_serial(serial.data()); //(const char *serial);
}

std::string crtlsdr::get_device_serial(uint32_t index){
	char manufact[256];
	char product[256];
	char serial[256];
	int  ret = rtlsdr_get_device_usb_strings(index,manufact,product,serial);
	if (ret<0){
		std::cerr << "get_device_serial() failed for #"<< std::to_string(index) << std::endl;
		return "";
	}

	return serial;
}

std::string crtlsdr::get_usb_str_concat(uint32_t index){
	char manufact[256];
	char product[256];
	char serial[256];
	int  ret = rtlsdr_get_device_usb_strings(index,manufact,product,serial);
	if (ret<0){
		std::cerr << "get_device_serial() failed for #"<< std::to_string(index) << std::endl;
		return "";
	}

	return std::string(serial) +" : " + std::string(product) + " : " + std::string(manufact);
}

int crtlsdr::open(std::string name){
	return open(crtlsdr::get_index_by_serial(name.data()));
}

int crtlsdr::open(uint32_t index){

	int ret;
	devnum = index;
	//std::cout << "opening #" << std::to_string(index) << std::endl;
	ret = rtlsdr_open(&dev,index);
	if (ret!=0) return ret;
	ret = set_samplerate(samplerate);
	if (ret!=0) return ret;
	ret = rtlsdr_set_dithering(dev, 0);  //THIS MUST PRECEDE THE TUNING FREQ CALL, OTHERWISE IT WILL FAIL!
	if (ret!=0) return ret;
	ret = set_fcenter(fcenter);
	if (ret!=0) return ret;
	ret = set_agcmode(enableagc);
	if (ret!=0) return ret;
	ret = set_tunergainmode(1);
	if (ret!=0) return ret;
	ret = set_tunergain(rfgain);
	if (ret!=0) return ret;
	ret = set_correction_f(0.0f);  //this must be zeroed. value is retained after close...

	devname = crtlsdr::get_device_serial(devnum);
	return ret;
}

int crtlsdr::close(){
	return rtlsdr_close(dev);
}


int crtlsdr::set_fcenter(uint32_t f){
	fcenter=f;
	rtlsdr_set_dithering(dev, 0);
	return rtlsdr_set_center_freq(dev,fcenter);
}

int crtlsdr::set_samplerate(uint32_t fs){
	samplerate = fs;
	return rtlsdr_set_sample_rate(dev,fs);
}

int crtlsdr::set_agcmode(bool flag){
	uint32_t agc = (flag) ? 1 : 0;
	return rtlsdr_set_agc_mode(dev, agc);
}

int crtlsdr::set_tunergain(uint32_t gain){
	rfgain = gain;
	return rtlsdr_set_tuner_gain(dev, gain);
}

int crtlsdr::set_tunergainmode(uint32_t mode){
	return rtlsdr_set_tuner_gain_mode(dev, mode);
}

int crtlsdr::set_correction_f(float f){
	//std::cout << "set correction on dev " << std::to_string(devnum) << ":" << std::to_string((long int )dev) << std::endl;
	return ((dev!=NULL) && (!do_exit)) ? rtlsdr_set_sample_freq_correction_f(dev,f) : -1;
}


int8_t* crtlsdr::swapbuffer(uint8_t *b){
	//auto t = std::chrono::high_resolution_clock::now();
	//auto t_ns= (std::chrono::time_point_cast<std::chrono::nanoseconds>(t)).time_since_epoch();

	//if (streaming){
		{
			    std::unique_lock<std::mutex> lock(mtx);
				s8bit.setbufferptr(b,get_readcnt()); //postincremented
				inc_readcnt(); //was included in mutex scope.

		
		 		
		 		//cdsp::convtosigned(b, s8bit, blocksize);
		 		//s8bit.readcnt = inc_readcnt();
		 		//s8bit.timestamp= t_ns.count();
		 		newdata++; //newdata=true;
		}
	//}
	cv.notify_all();
	return s8bit.getbufferptr();
}

int8_t* crtlsdr::read(){
	if (streaming){
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait(lock, [this]{return (newdata.load()>0);});
		newdata--; //false;
		return s8bit.getbufferptr(); //this has to be inside mutex scope, otherwise swapbuffer may modify reaccnt and we return wrong buffer occasionally.
	}
	return s8bit.getbufferptr(); // double return statements, in case streaming is false.
}

const std::complex<float> *crtlsdr::convtofloat(){
	return cdsp::convtofloat(sfloat,s8bit.getbufferptr(),blocksize);
}


const std::complex<float> *crtlsdr::convtofloat(const std::complex<float> *p){
	return cdsp::convtofloat(p,s8bit.getbufferptr(),blocksize);
}


const std::complex<float> *crefsdr::convtofloat(){
	cdsp::convtofloat(sfloat+(blocksize>>1),s8bit.getbufferptr(),blocksize);
	return sfloat + (blocksize>>1);
}

const std::complex<float> *crefsdr::convtofloat(const std::complex<float> *p){
	cdsp::convtofloat(p+(blocksize>>1),s8bit.getbufferptr(),blocksize);
	return p + (blocksize >> 1);
}

void crefsdr::start(barrier *b){
	startbarrier = b;
	thread = std::thread(crtlsdr::asynch_threadf,this);
	streaming = true;
	//controller->start();
}

const std::complex<float>* crefsdr::get_sptr(){
	return sfloat;// + (blocksize >> 1);
}