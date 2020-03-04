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

#include <vector>
#include <cmath>
#include <boost/tuple/tuple.hpp>
#include  <unistd.h>
#include "gnuplot-iostream.h"
#include <zmq.hpp>
#include <iostream>

#include "Eigen/Dense"
#include "Eigen/KroneckerProduct"
#include <complex>
#include <zmq.hpp>
#include <thread>
#include <mutex>

#include "../include/cdsp.h"
#include <volk/volk.h>

#include "mat.h"

using namespace std;
using namespace Eigen;

#define MX 7
#define MY 3
#define NSENSOR MX*MY


typedef Matrix<complex<float>,Dynamic,Dynamic> MatrixType;
typedef Map<MatrixType> MapType;
typedef Map<const MatrixType> MapTypeConst;
typedef Matrix<complex<float>,NSENSOR,1> ColVecType;

typedef Matrix<complex<float>,1,NSENSOR> RowVecType;

typedef Matrix<complex<float>,Dynamic,1> ColVecTypeD; //try the dynamic.

typedef std::pair<float, float> Point;

typedef std::vector<Point> Point2d;

const complex<float> j(0, 1);
const float pi = std::acos(-1);

//g++ mattoeigen.cpp -lmat -lmx -lutil -lboost_iostreams -lboost_system -lboost_filesystem -I ~/source/eigen-3.3.7/Eigen/ -I /usr/local/MATLAB/R2019b/extern/include/ -L /usr/local/MATLAB/R2019b/bin/glnxa64/ -I ~/source/gnuplot-iostream/ -Wl,-rpath=/usr/local/MATLAB/R2019b/bin/glnxa64/

// with the unsupported Kronecker product patched in do:

//g++ heatmap2d2.cpp -lmx -lmat -lvolk -lpthread -lzmq -lutil -lboost_iostreams -lboost_system -lboost_filesystem -I ~/source/eigen-3.3.7/ -I ~/source/tmp/eigen/unsupported/ -I ~/source/gnuplot-iostream/ -I /usr/local/MATLAB/R2019b/extern/include/ -L /usr/local/MATLAB/R2019b/bin/glnxa64/ -Wl,-rpath=/usr/local/MATLAB/R2019b/bin/glnxa64/

//Find noise subspace from Covariance matrix Rxx, assuming K signals. Use SVD.
Matrix<complex<float>,Dynamic,Dynamic> noisesubspace(Matrix<complex<float>,Dynamic,Dynamic> Rxx, int K){
	int M = Rxx.cols();
	BDCSVD<Matrix<complex<float>,Dynamic,Dynamic>> B=Rxx.bdcSvd(ComputeFullU|ComputeFullV);
	Matrix<complex<float>,Dynamic,Dynamic> U  = B.matrixU();
	Matrix<complex<float>,Dynamic,Dynamic> Un = U.rightCols(M-K);
	Matrix<std::complex<float>,1,Dynamic> S   = B.singularValues();
	//cout << "singularvalues: " << S << endl;
	return Un;
}

//Noise subspace with Eigendecomp. This cannot be used as such, because the eigenvectors&vals are unordered.
MatrixType noisesubspaceeig(MatrixType Rxx, int K){
	SelfAdjointEigenSolver<MatrixType> es;
	es.compute(Rxx);
	//cout << "The eigenvalues of Rxx are: " << es.eigenvalues().transpose() << endl;
	//cout << "The matrix of eigenvectors, V, is:" << endl << es.eigenvectors() << endl << endl;
	return es.eigenvectors();
}

//steering vector, for direction phi(rad)
ColVecType s_vec(float phi){
	ColVecType A;
	
	float d = (1.225*1.24)/3.0f; //element distance, wavelengths
	//d = 0.5f; //exp(2j*pi*d*cos(pi))
	
	for (int i=0;i<NSENSOR;i++)
		A(i) = std::exp(2.0f*pi*j*cos(phi)*float(i)*d);
	return A;
}

ColVecTypeD s_vecd2d(float alpha, float beta, float d, int Mx, int My){
	ColVecTypeD res(Mx*My,1);
	int rc=0;

	for (int iy=0;iy<My;iy++){
		for(int ix=0;ix<Mx;ix++){
			res(rc++) = exp(2.0f*pi*j*float(ix)*d*cos(alpha)*sin(beta))* //sin cos
						exp(2.0f*pi*j*float(iy)*d*cos(beta)); // cos sin
		}
	}

	return res;
}



float pmusic(Matrix<complex<float>,Dynamic,Dynamic> Un, Matrix<complex<float>,Dynamic,1> a)
{
	//complex<float> denom = ((Un.adjoint()*a).norm());
	complex<float> denom = (Un.adjoint()*a).squaredNorm();
	complex<float> res   = a.squaredNorm()/denom;
	return (res*conj(res)).real();
	//complex<float> denom = a.adjoint()*Un*Un.adjoint()*a;
	//return (1.0f/denom).real();
}

/*
Matrix<complex<float>,Dynamic,Dynamic> DA(Matrix<complex<float>,Dynamic,Dynamic>){
	Matrix<<complex<float>,Dynamic,Dynamic> res;
	res.resize();
}*/

Matrix<float,Dynamic,Dynamic> pmusic2dvec(Matrix<complex<float>,Dynamic,Dynamic> Un,float d, int Mx, int My, int Cx, int Cy){
	Matrix<float,Dynamic,Dynamic> res(Cx,Cy);
	for(int cx=0;cx<Cx;cx++)
		for(int cy=0;cy<Cy;cy++){
			auto a = s_vecd2d(float(cx)*pi/float(Cx),float(cy)*pi/float(Cy),d,Mx,My);
			res(cx,cy) = pmusic(Un,a);
		}

	return res;
}


Matrix<float,Dynamic,1> pmusicvec(Matrix<complex<float>,Dynamic,Dynamic> Un, int N) //N+1 points between 0 to PI
{
	Matrix<float,Dynamic,1> res;
	res.resize(N,1);

	for(int n=0;n<N;n++){
		ColVecType A = s_vec(pi*float(n)/float(N));
		res(n) 		 = pmusic(Un,A);
	}
	return res;
}

complex<float> *dbuf0;
complex<float> *dbuf1;
bool	exit_all;
uint32_t gseq;
std::mutex g_i_mutex;

struct cdims{
	uint32_t cols;
	uint32_t rows;
	cdims(uint32_t r_,uint32_t c_):rows(r_), cols(c_){};
};

void plotthread(cdims *dims){

	uint32_t cols = dims->cols;
	uint32_t rows = dims->rows;

	Gnuplot gp;
	gp << "set hidden3d nooffset\n";
	gp << "set style fill transparent solid 0.65\n";

	//preallocate 2d vector container for gnuplotc++ interface:
	std::vector<std::vector<std::vector<float> > > pts(100);
	for (int nx=0;nx<100;nx++){
		pts[nx].resize(100);
		for (int ny=0;ny<100;ny++){
			pts[nx][ny].resize(3);			
		}
	}
	MapType Xall(dbuf0,rows,cols);
	MapType Xall2(dbuf1,rows,cols);

	while (!exit_all){
		MatrixType X = Xall.rightCols(cols-1);
		X.rowwise()-=X.colwise().mean();
		
		{
			const std::lock_guard<std::mutex> lock(g_i_mutex);
			std::swap(dbuf0,dbuf1);
			std::swap(Xall,Xall2);
		}

		// covariance matrix estimate & noise subspace via SVD
	  	MatrixType Rxx = (1/float(rows))*(X.adjoint()*X);
		MatrixType Un = noisesubspace(Rxx,1);

		float d = (1.225*1.24)/3.0f;
		Matrix<float,Dynamic,Dynamic> pm = pmusic2dvec(Un,d,MX,MY,100, 100);

		//normalize
		auto g = pm.maxCoeff();
		pm *= 1/g;
		for (int nx=0;nx<100;nx++){
			for (int ny=0;ny<100;ny++){
				pts[nx][ny][0] = -90 + float(nx)*1.8f;
				pts[nx][ny][1] = -90 + float(ny)*1.8f;
				pts[nx][ny][2] = 20.0f*log10(pm(nx,ny)); //2.0f;
				
			}
		}
		gp.clear();
		gp << "set yrange [-90:90]\n";
		gp << "set xrange [-90:90]\n";
		gp << "set cbrange [-20:0]\n";
		gp << "set zrange [-20:1]\n";
		//gp << "set view map\n";
		gp << "splot '-' with pm3d title 'Pm " << to_string(gseq)<<"'\n";
		gp.send2d(pts);
		gp.flush();
	}

}

void matsave(int8_t *s8bit, uint32_t r, uint32_t c, string fname){
	// get the time as string
	time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char s[64];
    strftime(s, sizeof(s), "%c", tm);

	cout <<  "Creating file " << fname << endl;
	MATFile *pmat = matOpen(fname.data(),"w");

	mxArray *pa1  = mxCreateNumericMatrix(r,c,mxSINGLE_CLASS,mxCOMPLEX);
	mxArray *pa2  = mxCreateString(s);
	
	//get the pointer to our matlab matrix and convert from i8
	float* outptr = (float *) mxGetComplexSingles(pa1);
	volk_8i_s32f_convert_32f((float *) outptr, s8bit,127.0f,2*r*c);

	matPutVariable(pmat, "x", pa1);
	matPutVariable(pmat, "time", pa2);
	mxDestroyArray(pa1);
    mxDestroyArray(pa2);

	matClose(pmat);
}


void usage(void)
{
	fprintf(stderr,
		"\n URA beamform & Matlab .mat save:\n"
		"\t[-a receiver address, e.g. tcp://localhost:5555\n"
		"\t[-c frame count, 0 = inf]\n"
		"\t[-f filename to save, if given sets framecount = 1]\n");
	exit(1);
}

int main(int argc, char **argv) {

	//set defaults & parse command line
	string addr = "tcp://localhost:5555";
	string fname;
	int totalframes = 1;

	int opt;
	while ((opt = getopt(argc, argv, "a:c:f:h")) != -1) {
		switch (opt) {
			case 'a':
				addr = string(optarg);
			break;
			case 'c':
				totalframes = (int) atoi(optarg);
			break;
			case 'f':
				fname = string(optarg);
				totalframes = 1;
			break;
			case 'h':
			default:
				usage();
			break;
		}
	}


	//set up zmq and retrieve one frame to determine dimensions
	zmq::context_t ctx(1);
	zmq::socket_t sock(ctx, ZMQ_SUB);
	sock.setsockopt(ZMQ_SUBSCRIBE, "", 0);
	sock.connect(addr.data());

	zmq::message_t msg;
	sock.recv (&msg);

	int8_t *s8bit =  static_cast<int8_t*>(msg.data());
  	gseq = *((uint32_t *) s8bit);
  	uint32_t cols = *(((uint32_t *) s8bit)+1);
  	uint32_t rows = *(((uint32_t *) s8bit)+2);
  	//cols = NSENSOR; //only the first antennas.

  	cout << "Allocating a " << to_string(rows) << " X " << to_string(cols) << " matrix" << endl;

	//raw data buffer for incoming sample conversion:
	dbuf0 = new complex<float> [rows*cols];
	dbuf1 = new complex<float> [rows*cols];
	volk_8i_s32f_convert_32f((float *) dbuf0, s8bit,127.0f,rows*cols*2);

	exit_all = false;
	cdims dim(rows,cols);
	std::thread plotthrd(plotthread,&dim);


	//main loop simply receives data and converts to 32-bit float
	for (int n=0;n<totalframes || totalframes==0;n++){
		sock.recv (&msg);
		s8bit =  static_cast<int8_t*>(msg.data());
		gseq = *((uint32_t *) s8bit);
		s8bit +=16+sizeof(uint32_t)*cols;
		//convert samples to complex float, use a scoped lock in case plotthread is switching buffers.
		{
			//cdsp::convtofloat(dbuf, s8bit, rows*cols*2); //maybe someday include this in cmake such that we can use this...
			const std::lock_guard<std::mutex> lock(g_i_mutex);
			volk_8i_s32f_convert_32f((float *) dbuf0, s8bit,127.0f,rows*cols*2);
		}
	}

	// save the file as matlab mat. we'll have to reconvert from int8 since
	// plotthrd might have swapped buffers
	if (!fname.empty()){
		exit_all = true;
		matsave(s8bit,rows,cols,fname);
	}


	plotthrd.join();

	delete[] dbuf0;
	delete[] dbuf1;
}