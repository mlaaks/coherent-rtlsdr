cmake_minimum_required(VERSION 2.6)

project(REFNOISEFW C ASM)

set(CMAKE_ASM_COMPILER arm-none-eabi-as)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)


#optimization and architecture:
	set(OPT "-Os")
	set(ARCH_FLAGS "-mcpu=cortex-m3 -mthumb -msoft-float -mfix-cortex-m3-ldrd")
	SET(SECT_FLAGS "-fno-common -ffunction-sections -fdata-sections")


	add_definitions(-DSTM32F1)

#linker defs:
    set(LIBOPENCM3_DIR ${CMAKE_SOURCE_DIR}/libopencm3)
	
	set(LINKERSCRIPT ${CMAKE_SOURCE_DIR}/stm32-bluepill.ld)
	set(LINKERSCRIPTB ${CMAKE_SOURCE_DIR}/stm32-bluebootld.ld)
	set(LINKERSCRIPTDFU ${CMAKE_SOURCE_DIR}/stm32-bluepilldfu.ld)

	set(LD_FLAGS "--static -nostartfiles -L${LIBOPENCM3_DIR}/lib/  -Wl,--gc-sections -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group")
    LINK_DIRECTORIES(${LIBOPENCM3_DIR}/lib)
	

#includes & sources
	include_directories(
		${LIBOPENCM3_DIR}/include
	)

	set(FWSOURCES fw.c)
	set(BLSOURCES bootloader.c)
	set(DFUAPPSOURCES usbdfu.c)


#build
add_executable(REFNOISEFW ${FWSOURCES})
add_executable(BOOTLOADER ${BLSOURCES})
add_executable(DFUAPP ${DFUAPPSOURCES})
	
#output defs, flags, for firmware:
	set_target_properties(REFNOISEFW PROPERTIES OUTPUT_NAME "refnoisefw.elf")
	target_link_libraries(REFNOISEFW opencm3_stm32f1 "-T${LINKERSCRIPT}")
	set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS) #-rdynamic
	set(CMAKE_C_LINK_FLAGS)
	set(CMAKE_EXE_LINKER_FLAGS "${LD_FLAGS}")
	set(CMAKE_C_FLAGS "${OPT} ${ARCH_FLAGS} ${SECT_FLAGS}")

#same for bootloader
	set_target_properties(BOOTLOADER PROPERTIES OUTPUT_NAME "bootloader.elf")
	target_link_libraries(BOOTLOADER opencm3_stm32f1 "-T${LINKERSCRIPTB}")

#same for dfuapp
	set_target_properties(DFUAPP PROPERTIES OUTPUT_NAME "dfuapp.elf")
	target_link_libraries(DFUAPP opencm3_stm32f1 "-T${LINKERSCRIPTDFU}")

#convert both to binaries, could also do ihex format...
add_custom_command(TARGET REFNOISEFW
    POST_BUILD
    COMMAND arm-none-eabi-objcopy -O binary refnoisefw.elf refnoisefw.bin
    COMMAND arm-none-eabi-objcopy -O ihex refnoisefw.elf refnoisefw.hex
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/build
)

add_custom_command(TARGET BOOTLOADER
    POST_BUILD
    COMMAND arm-none-eabi-objcopy -O binary bootloader.elf bootloader.bin
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/build
)

add_custom_command(TARGET DFUAPP
	POST_BUILD
	COMMAND arm-none-eabi-objcopy -O binary dfuapp.elf dfuapp.bin
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

add_custom_target(flash
    COMMENT *FLASHING FW, BOOTLOADER & DFUAPP*
    COMMAND st-flash write refnoisefw.bin 0x08002000
    COMMAND st-flash write bootloader.bin 0x08000000
    COMMAND st-flash write dfuapp.bin 0x08000400
    DEPENDS REFNOISEFW
)

add_custom_target(flash_bootloader
    COMMENT *FLASHING_BOOTLOADER & DFUAPP*
    COMMAND st-flash write bootloader.bin 0x08000000
    DEPENDS REFNOISEFW
)

add_custom_target(dfu_flash
	COMMENT *FLASHING_FW_VIA_USB_DFU*
	COMMAND ../enter_dfu.sh
	COMMAND dfu-util -d 0483:df11 -a 0 -s 0x08002000:leave -D refnoisefw.bin
)