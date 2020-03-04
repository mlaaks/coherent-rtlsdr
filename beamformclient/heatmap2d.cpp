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
//#include "mat.h"

using namespace std;
using namespace Eigen;


#define NSENSOR 18

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

//g++ mattoeigen.cpp -lzmq -lutil -lboost_iostreams -lboost_system -lboost_filesystem -I ~/source/eigen-3.3.7/ -I ~/source/tmp/eigen/unsupported/ -I ~/source/gnuplot-iostream/

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
/*
ColVecTypeD s_vecd2d(float alpha,float beta, float d, int Mx, int My){
	ColVecTypeD ax(Mx,1); //does this work as a.resize(M,1);?
	ColVecTypeD ay(My,1);
	//ColVecTypeD res(Mx*My,1);

	for (int i=0;i<Mx;++i){
		ax(i) == exp(2.0f*pi*j*float(i)*d*cos(alpha)*cos(beta));
	}
	for (int i=0;i<My;++i){
		ay(i) == exp(2.0f*pi*j*float(i)*d*sin(alpha)*cos(beta));
	}
	return kroneckerProduct(ay,ax).eval(); //or, instead could roll out the loop and do this withoud KP
}
*/
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


// TODO : Find the ZMQ-impl of this file, copy&paste it in place of matlab open. Implement crude command line. Intensity plot or 3dplot?

int main(int argc, char **argv) {
	
	//set up zmq and retrieve one frame to determine dimensions
	zmq::context_t ctx(1);
	zmq::socket_t sock(ctx, ZMQ_SUB);
	sock.setsockopt(ZMQ_SUBSCRIBE, "", 0);
	sock.connect("tcp://localhost:5555");

	zmq::message_t msg;
	sock.recv (&msg);

	int8_t *s8bit =  static_cast<int8_t*>(msg.data());
  	uint32_t gseq = *((uint32_t *) s8bit);
  	uint32_t cols = *(((uint32_t *) s8bit)+1);
  	uint32_t rows = *(((uint32_t *) s8bit)+2);
  	//cols = NSENSOR; //only the first antennas.

  	cout << "Allocating a " << to_string(rows) << " X " << to_string(cols) << " matrix" << endl;

	MatrixType Xall(rows,cols); //MapType Xall(matptr,rows,cols);
	Xall.resize(rows,cols);

	Gnuplot gp;

	//gp << "set zrange [-1:1]\n";
	gp << "set hidden3d nooffset\n";
	gp << "set style fill transparent solid 0.65\n";
	//gp << "set view map\n";

	vector<Point> data;

	vector<Point2d> data2d;
	 
	int nrep = 10000;

	for (int nn=0; nn<nrep;nn++){
		sock.recv (&msg);
		int8_t *s8bit =  static_cast<int8_t*>(msg.data());
		//convert samples to complex float:
		//cout << "converting data\n" << endl;
		if (nn%1==0){  
			float s = 1/127.0f;
			for(uint32_t c=0;c<cols;c++) 			//start from 1, since channel 0 is reference noise
			  	for(uint32_t r=0, ri=0;r<rows;r++,ri++){		//interleaved i&q parts
			  		Xall(r,c) = s*complex<float>(float(s8bit[c*(rows<<1) + ri++]),float(s8bit[c*(rows<<1) + ri]));
			  	}


			//cout << "subtracting mean\n" << endl;  
			MatrixType X = Xall.rightCols(cols-1); //.leftCols(NSENSOR);
			X.rowwise()-=X.colwise().mean(); //subtract col mean 

			//cout << "Computing noise subspace\n" << endl;
		  	MatrixType Rxx = (1/float(rows))*(X.adjoint()*X);
			MatrixType Un = noisesubspace(Rxx,1);

			//cout << "Computing music\n" << endl;

			//Matrix<float,Dynamic,1> pm = pmusicvec(Un,100);

			float d = (1.225*1.24)/3.0f;
			Matrix<float,Dynamic,Dynamic> pm = pmusic2dvec(Un,d,6,3,100, 100);

			//pm.col(50) = pmusicvec(Un,100);

			auto g = pm.maxCoeff();
			//cout << g << endl;
			pm *= 1/g;

			std::vector<std::vector<std::vector<float> > > pts(100);
			for (int nx=0;nx<100;nx++){
				pts[nx].resize(100);
				for (int ny=0;ny<100;ny++){
					pts[nx][ny].resize(3);
					pts[nx][ny][0] = -90 + float(nx)*1.8f;
					pts[nx][ny][1] = -90 + float(ny)*1.8f;
					pts[nx][ny][2] = 20.0f*log10(pm(nx,ny)); //2.0f;
					
				}
			}
			gp.clear();
			gp << "set yrange [-90:90]\n";
			gp << "set xrange [-90:90]\n";
			gp << "set cbrange [-18:0]\n";
			gp << "set zrange [-18:1]\n";
			//gp << "set view map\n";
			gp << "splot '-' with pm3d title 'Pm'\n";
			gp.send2d(pts);
			gp.flush();
			//gp << gp.binFile2d(pts, "record") << "with lines title 'vec vec vec'";

			/*
			for(int n=0;n<101;n++){
			//data.push_back(Point(180.0f/pi*float(n)/10.0f,pm(n)));
			data.push_back(Point(float(n),10.0f*log10(pm(n))));
			}
		
			//cout << "Dim X:" << X.rows() << "," << X.cols() << endl;	  
			gp.clear();
			gp << "unset border\n";
			//gp << "set yrange[0:10]\n";
			gp << "set yrange[-20:1]\n";
			gp << "plot '-' with lines title 'Pm'\n";
			gp.send1d(data);
			gp.flush();

			data.clear();*/
		}
	/*
		//WORKING MUSIC
	  	for (int phi=0;phi<1800;phi++){
		  ColVecType A = s_vec(pi*float(phi)/1800.0f);
		  //cout << A.transpose() << endl;
		  data.push_back(Point(float(phi),10*log10(pmusic(Un,A))));
		}
	*/
	}

}

//Map<MatrixType> X(NULL);
  	//new (&A) Map<Matrix3f>(get_matrix_pointer(i));
  	//new (&X) Map<Matrix<complex<float>,Dynamic,Dynamic>>(outptr,dims[0],dims[1]); 
  	//&XMap<Matrix<int,2,4> >(array)
