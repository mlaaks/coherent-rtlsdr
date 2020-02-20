# coherent-rtlsdr, reworked synchronization algorithm

Mostly the same requirements as in my previous coherentsdr proof-of-concept. One added dependency, GNU Readline, for the shell. Some unworking features: adding and deleting receivers during runtime, application does not always exit cleanly. 

Matlab client included. A system object interfacing to a .c MEX implementation (matlab c++ interfacing seems to be too slow). This is not built automatically by cmake, instead it has to be compiled manually (instructions in the folder).

Required for compiling:

:zmq: - the zero message queue:
	sudo apt-get install libzmq3-dev

:fftw3f: - fastest fourier transform in the west:
	sudo apt-get install fftw3-dev

:volk: - vector optimized library of kernels:
	sudo apt-get install volk
	
+GNU Readline

Requires common 28.8 MHz clock & reference signal (noise) for synchronization. Some examples in the electronics folder, but this is still missing the coupler module PCB files.

Currently may return a receive matrix where data on some channels are from a previous sample buffer. This seems to happen under heavy CPU load, at least on limited platforms (testing on RockPI 4 with 21 signal channels). This can be noticed by observing discontinuous channel sequence number (readcnt). Occasionally, all channels skip a buffer in unison, i.e. 8192 sample gap in reception. Perhaps I may need to add some buffering to the packetizer singleton. Under construction.
