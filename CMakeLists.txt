cmake_minimum_required(VERSION 3.16)

project(CRC_KIT LANGUAGES C)

add_executable(crc_kit main.c)

include(CheckFunctionExists)
check_function_exists(stpcpy HAVE_STPCPY)
if (HAVE_STPCPY)
target_compile_definitions(crc_kit PRIVATE -DHAVE_STPCPY)
endif ()
check_function_exists(strcasecmp HAVE_STRCASECMP)
if (HAVE_STRCASECMP)
target_compile_definitions(crc_kit PRIVATE -DHAVE_STRCASECMP)
endif ()

include(GNUInstallDirs)
install(TARGETS crc_kit
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
