# coherent-rtlsdr, reworked

Mostly the same requirements as in the previous coherentsdr proof-of-concept. One added dependency, GNU Readline, for the shell.

:zmq: - the zero message queue
	sudo apt-get install libzmq3-dev

:fftw3f: - fastest fourier transform in the west
	sudo apt-get install fftw3-dev

:volk: - vector optimized library of kernels
	sudo apt-get install volk
