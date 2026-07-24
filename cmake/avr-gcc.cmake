# find the toolchain root directory
if(UNIX)
    set(OS_SUFFIX "")
    find_path(TOOLCHAIN_ROOT
        NAMES
            avr-gcc${OS_SUFFIX}
        PATHS
            /usr/bin/
            /usr/local/bin
            /bin/
            $ENV{AVR_ROOT}
    )
elseif(WIN32)
    set(OS_SUFFIX ".exe")
    find_path(TOOLCHAIN_ROOT
        NAMES
            avr-gcc${OS_SUFFIX}
        PATHS
            "C:\\WinAVR\\bin"
            $ENV{AVR_ROOT}
    )
else(UNIX)
    message(FATAL_ERROR "toolchain not supported on this OS")
endif(UNIX)

if(NOT TOOLCHAIN_ROOT)
    message(FATAL_ERROR "Toolchain root could not be found!!!")
endif(NOT TOOLCHAIN_ROOT)

# setup the AVR compiler variables

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)
set(CMAKE_CROSS_COMPILING 1)

set(CMAKE_C_COMPILER   "${TOOLCHAIN_ROOT}/avr-gcc${OS_SUFFIX}"     CACHE PATH "gcc"     FORCE)
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_ROOT}/avr-g++${OS_SUFFIX}"     CACHE PATH "g++"     FORCE)
set(CMAKE_AR           "${TOOLCHAIN_ROOT}/avr-ar${OS_SUFFIX}"      CACHE PATH "ar"      FORCE)
set(CMAKE_LINKER       "${TOOLCHAIN_ROOT}/avr-ld${OS_SUFFIX}"      CACHE PATH "linker"  FORCE)
set(CMAKE_NM           "${TOOLCHAIN_ROOT}/avr-nm${OS_SUFFIX}"      CACHE PATH "nm"      FORCE)
set(CMAKE_OBJCOPY      "${TOOLCHAIN_ROOT}/avr-objcopy${OS_SUFFIX}" CACHE PATH "objcopy" FORCE)
set(CMAKE_OBJDUMP      "${TOOLCHAIN_ROOT}/avr-objdump${OS_SUFFIX}" CACHE PATH "objdump" FORCE)
set(CMAKE_STRIP        "${TOOLCHAIN_ROOT}/avr-strip${OS_SUFFIX}"   CACHE PATH "strip"   FORCE)
set(CMAKE_RANLIB       "${TOOLCHAIN_ROOT}/avr-ranlib${OS_SUFFIX}"  CACHE PATH "ranlib"  FORCE)
set(AVR_SIZE           "${TOOLCHAIN_ROOT}/avr-size${OS_SUFFIX}"    CACHE PATH "size"    FORCE)

set(CMAKE_C_FLAGS_DEBUG "-DDEBUG")

# avr uploader config
find_program(AVR_UPLOAD
    NAME
        avrdude

    PATHS
        /usr/bin/
        $ENV{AVR_ROOT}
)

function(add_avr_executable)
    set(oneValueArgs TARGET OUTPUT_NAME MCU)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(AVR_EXEC "" "${oneValueArgs}" "${multiValueArgs}" ${ARGV})

    set(ELF_TARGET ${AVR_EXEC_TARGET}-elf)
    set(MAP_TARGET ${AVR_EXEC_TARGET}-map)
    set(HEX_TARGET ${AVR_EXEC_TARGET}-hex)

    set(ELF_FILE ${AVR_EXEC_OUTPUT_NAME}.elf)
    set(MAP_FILE ${AVR_EXEC_OUTPUT_NAME}.map)
    set(HEX_FILE ${AVR_EXEC_OUTPUT_NAME}.hex)

    # create elf file
    add_executable(${ELF_TARGET}
        ${AVR_EXEC_SOURCES}
    )

    target_compile_options(${ELF_TARGET}
        PRIVATE
            -x c
            $<$<CONFIG:Debug>:-O2>
            $<$<CONFIG:Release>:-Os>
            -ffunction-sections
            -fdata-sections
            $<$<CONFIG:Debug>:-g3>
            -Wall
            -mmcu=${AVR_EXEC_MCU}
            # -B"<path>/XMEGAA_DFP/<version>/gcc/dev/${AVR_EXEC_MCU}"
            -std=gnu99
            -fno-strict-aliasing
            -Wstrict-prototypes
            -Wmissing-prototypes
            -Werror-implicit-function-declaration
            -Wpointer-arith
            -mrelax
            $<$<CONFIG:Release>:-lc>
            # -MD -MP -MF"<filepath>.d" -MT"<filepath>.d" -MT"<filepath>.o"
    )

    target_link_options(${ELF_TARGET}
        PRIVATE
            -Wl,-Map,${MAP_FILE}
            -Wl,--start-group
            -Wl,-lm
            -Wl,--end-group
            -Wl,--gc-sections
            -mmcu=${AVR_EXEC_MCU}
            # -B"<path>/XMEGAA_DFP/<version>/gcc/dev/${AVR_EXEC_MCU}"
            -Wl,--relax
            ${AVR_LINKER_LIBS}
    )

    set_target_properties(${ELF_TARGET}
        PROPERTIES
            OUTPUT_NAME ${ELF_FILE}
    )

    # create hex file
    add_custom_command(
        OUTPUT ${HEX_TARGET}
        COMMAND ${CMAKE_OBJCOPY} -O ihex -R .eeprom -R .fuse -R .lock -R .signature -R .user_signatures ${ELF_FILE} ${HEX_FILE}
        DEPENDS ${ELF_TARGET}
    )
    
    # build the intel hex file for the device
    add_custom_target(
        ${AVR_EXEC_TARGET}
        ALL
        DEPENDS ${HEX_TARGET}
    )
        
    set_target_properties(
        ${AVR_EXEC_TARGET}

        PROPERTIES
            OUTPUT_NAME ${ELF_FILE}
    )
endfunction(add_avr_executable)
