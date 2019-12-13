cmake_minimum_required (VERSION 2.6)


option(RASPBERRYPI "RPI" OFF)
if (RASPBERRYPI)
	add_definitions(-DRASPBERRYPI)
endif(RASPBERRYPI)

#set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-O3") #-Wl,-Bdynamic

include_directories(${PROJECT_SOURCE_DIR}/include)
add_subdirectory(src)



#SET_TARGET_PROPERTIES(coherentrtlsdr PROPERTIES LINK_FLAGS -lrtlsdr)