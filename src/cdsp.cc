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

#include "cdsp.h"
// currently wraps to volk kernels. In future, these could be mapped to custom code.

void cdsp::convtosigned(const uint8_t *in, const uint8_t *out, int n){
	//now the compiler may or may not optimize this to use 32 or 64 -bit pointers, xorring multiple samples (4, or 8) simultaneously
	//should check, if not, then write the code manually
	//for(int i=0;i<n;++i)
	//	out.ptr[i]=in.ptr[i] ^ 0x80;

	//only use blocksizes of 2^N for this
	uint64_t *iptr = (uint64_t *) in;
	uint64_t *optr = (uint64_t *) out;
	int N = n >> 3;

	for(int i=0;i<N;i++)
		optr[i] = iptr[i] ^ 0x8080808080808080;
}

const float* cdsp::convtofloat(const float *out, const int8_t *s8bit,int n){
	volk_8i_s32f_convert_32f((float *) out, s8bit,127.0f,n);
	return out;
}

const std::complex<float>* cdsp::convtofloat(const std::complex<float> *out, const int8_t *s8bit,int n){
	volk_8i_s32f_convert_32f((float *) out, s8bit,127.0f,n);
	return out;
}

const std::complex<float>* cdsp::scalarmul(const std::complex<float> *out,const std::complex<float> *in,const std::complex<float> scalar_in, int n){
	volk_32fc_s32fc_multiply_32fc((lv_32fc_t *) out,(lv_32fc_t*) in,scalar_in,n);
	return out;
}

const std::complex<int8_t>* cdsp::convto8bit(std::complex<int8_t> *out,std::complex<float>*in, int n){
	volk_32f_s32f_convert_8i((int8_t *) out,(float *) in,127.0f,(n<<1));
	return out;
}
/*
const std::complex<float>* cdsp::convtofloat(const std::complex<float> *out, const int8_t *s8bit,int n){
	volk_8i_s32f_convert_32f((float *) out, s8bit,127.0f,n);
	return out;
}*/

const std::complex<float> cdsp::conj_dotproduct(const std::complex<float> *a,const std::complex<float> *b, int n){
    
    std::complex<float> c = 0.0f;
    volk_32fc_x2_conjugate_dot_prod_32fc((lv_32fc_t *) &c,(lv_32fc_t *) a,(lv_32fc_t *) b,n);
    return c;
}

const float cdsp::rms(const float *in, int n){
	float res;
	volk_32f_x2_dot_prod_32f(&res, in, in,n);
	return sqrt(res/n);
	//volk_32f_x2_dot_prod_32f(float* result, const float* input, const float* taps, unsigned int num_points)
}

const float cdsp::rms(const std::complex<float> *s, int n){
    std::complex<float> res = conj_dotproduct(s,s,n);
	return sqrt(res.real()/n);
}

const float cdsp::crestfactor(const float *in,float peak, int n){
	float r = rms(in,n);
	return(peak/r);
}

const float cdsp::PAPR(const std::complex<float> *s,const std::complex<float> *ref, int n){
    //float r = rms(in,n
    return 0; //FIXXXX (peak/r)*(peak/r);
}

const float cdsp::crestfactor(const float *in, int n){
	uint32_t idx;

	volk_32f_index_max_32u(&idx,in,n);
	float m = in[idx];
	float r = rms(in,n);

	return (m/r);
}

const float* cdsp::magsquared(float *out,const std::complex<float> *in,int n){
	volk_32fc_magnitude_squared_32f(out,in,n);
	return out;
}

const std::complex<float>* cdsp::conjugatemul(std::complex<float>*out, std::complex<float> *in1, std::complex<float> *in2, int n){
	volk_32fc_x2_multiply_conjugate_32fc(out,(const lv_32fc_t *) in1, (const lv_32fc_t *) in2,n); //result is in1 .* conj(in2)
	return out;
}

const std::complex<float>* cdsp::fft(std::complex<float>*out, std::complex<float> *in,fft_scheme *scheme){

#ifdef RASPBERRYPI
	gpu_fft_execute(*scheme);
#else
	fftwf_execute_dft(*scheme,(fftwf_complex*) in,(fftwf_complex *) out);
	//fftwf_execute(*scheme);
#endif

	return out;
}

const std::complex<float>* cdsp::fft(fft_scheme *scheme){

std::complex<float> *out;
#ifdef RASPBERRYPI
	gpu_fft_execute(*scheme);
	out = NULL;
#else
	fftwf_execute(*scheme);
	out = NULL;
#endif
	return out;
}

const uint32_t cdsp::indexofmax(float *in,int n){
	uint32_t idx;
	volk_32f_index_max_32u(&idx,in,n);
	return idx;
}

const uint32_t cdsp::indexofmax(float *out,std::complex<float> *in,int n){
	uint32_t idx;
	volk_32fc_magnitude_squared_32f(out,in,n);
	volk_32f_index_max_32u(&idx,out,n);
	return idx;
}
