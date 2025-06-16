cmake_minimum_required(VERSION 3.25)

include(cmake/avr-gcc.cmake)

project(Midi_Fighter_Twister C)

file(GLOB_RECURSE asf_sources lib/ASF/*.c)
file(GLOB_RECURSE lufa_sources lib/LUFA/*.c)
file(GLOB_RECURSE c_sources src/*.c)

# specify include toolchain file
add_avr_executable(
	TARGET midi-fighter-twister
	OUTPUT_NAME Midi_Fighter_Twister
	MCU atxmega128a4u
	SOURCES src/main.c ${asf_sources} ${lufa_sources} ${c_sources}
)

target_compile_definitions(midi-fighter-twister-elf
	PRIVATE
		F_USB=48000000UL
		F_CPU=32000000UL
		USE_LUFA_CONFIG_HEADER
		ARCH=ARCH_XMEGA
		BOARD=USER_BOARD
)

target_include_directories(midi-fighter-twister-elf
	PRIVATE
		lib/ASF/common/boards
		lib/ASF/common/boards/user_board
		lib/ASF/common/drivers/nvm
		lib/ASF/common/services/clock
		lib/ASF/common/services/delay
		lib/ASF/common/services/fifo
		lib/ASF/common/services/hugemem
		lib/ASF/common/services/ioport
		lib/ASF/common/services/serial
		lib/ASF/common/services/serial/xmega_usart
		lib/ASF/common/services/sleepmgr
		lib/ASF/common/utils
		lib/ASF/xmega/drivers/cpu
		lib/ASF/xmega/drivers/dma
		lib/ASF/xmega/drivers/nvm
		lib/ASF/xmega/drivers/pmic
		lib/ASF/xmega/drivers/sleep
		lib/ASF/xmega/drivers/tc
		lib/ASF/xmega/drivers/usart
		lib/ASF/xmega/drivers/wdt
		lib/ASF/xmega/utils
		lib/ASF/xmega/utils/preprocessor
		lib/LUFA/.
		src
		src/config
)
