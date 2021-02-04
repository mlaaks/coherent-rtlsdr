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
	
:librtlsdr: -Tejeez/Keenerds experimental librtlsdr fork:
	git://github.com/tejeez/rtl-sdr
	
+GNU Readline

Requires common 28.8 MHz clock & reference signal (noise) for synchronization. Some examples in the electronics folder, but this is still missing the coupler module PCB files.

Currently may return a receive matrix where data on some channels are from a previous sample buffer. This seems to happen under heavy CPU load, at least on limited platforms (testing on RockPI 4 with 21 signal channels). This can be noticed by observing discontinuous channel sequence number (readcnt). Occasionally, all channels skip a buffer in unison, i.e. 8192 sample gap in reception. Perhaps I may need to add some buffering to the packetizer singleton. Under construction.

UPDATE 2.2.2021: We have concentrated on experiments with the receiver and presented a paper at EUSIPCO2020. "Phase-coherent multichannel SDR - Sparse arraybeamforming" can be found at: https://aaltodoc.aalto.fi/handle/123456789/102361

Please cite this as: Laakso , M , Rajamäki , R , Wichman , R & Koivunen , V 2020 , Phase-coherent multichannel SDR - Sparse array beamforming . in 28th European Signal Processing Conference, EUSIPCO 2020 - Proceedings . , 9287664 , European Signal Processing Conference , EURASIP , pp. 1856-1860 , European Signal Processing Conference , Amsterdam , Netherlands , 24/08/2020 . https://doi.org/10.23919/Eusipco47968.2020.9287664

The next paper, in which we utilize deep neural networks on data captured with coherent-rtlsdr to do near-field localization (with surprisingly accurate results), will be presented at VTC2021 Helsinki.

Added the reference noise controller firmware, shamelessly edited from libopencm3 examples. This uses the 2$ STM32F103C8T6 (a.k.a bluepill) board to control 2 GPIOS for switching operating voltage for the noise amplifiers & case fan (yes, last summer I had problems with the amplifiers overheating while taking measurements during a sunny day). DFU upgradeable with dfu-util once flashed (hack, but works).

