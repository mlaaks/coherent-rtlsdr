project (coherentrtlsdr)

#include_directories(${PROJECT_SOURCE_DIR}/include)

set(SOURCES main.cc csdrdevice.cc crtlsdr.cc console.cc ccoherent.cc cdsp.cc ccontrol.cc cpacketizer.cc common.cc)

set(GPUFFT_DIR /opt/vc/src/hello_pi/hello_fft)

set(GPUFFT_SOURCES     ${GPUFFT_DIR}/mailbox.c
                       ${GPUFFT_DIR}/gpu_fft.c 
                       ${GPUFFT_DIR}/gpu_fft_base.c
                       ${GPUFFT_DIR}/gpu_fft_twiddles.c
                       ${GPUFFT_DIR}/gpu_fft_shaders.c)

if (RASPBERRYPI)
	include_directories(${GPUFFT_DIR})
       	include_directories(${GPUFFT_DIR}/hex)
	
	add_executable(coherentrtlsdr ${SOURCES} ${GPUFFT_SOURCES})
	target_link_libraries(coherentrtlsdr zmq pthread fftw3f volk rtlsdr readline stdc++ dl rt)
else (RASPBERRYPI)
	add_executable(coherentrtlsdr ${SOURCES})
	target_link_libraries(coherentrtlsdr zmq pthread fftw3f volk rtlsdr readline stdc++)
endif (RASPBERRYPI)

