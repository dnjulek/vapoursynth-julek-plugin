cmake_minimum_required(VERSION 3.20.0)

project(libjulek LANGUAGES CXX)

add_library(julek SHARED 
	src/shared.cpp
	src/Butteraugli.cpp
	src/RFS.cpp src/ssimulacra.cpp
	thirdparty/libjxl/tools/ssimulacra.cc
	thirdparty/libjxl/tools/ssimulacra2.cc
)

target_include_directories(julek PRIVATE
	thirdparty/libjxl
	thirdparty/libjxl/jxl_install/include
)

set_target_properties(julek PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

find_library(libjxl NAMES
	jxl
	jxl-static
	PATHS ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libjxl/jxl_install/lib
	NO_DEFAULT_PATH
)

message(STATUS "julek: find_library returned ${libjxl}")

find_library(libhwy NAMES
	hwy
	PATHS ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libjxl/jxl_install/lib
	NO_DEFAULT_PATH
)

message(STATUS "julek: find_library returned ${libhwy}")

target_link_libraries(julek PRIVATE
    ${libjxl}
    ${libhwy}
)

find_package(PkgConfig QUIET MODULE)

if(PKG_CONFIG_FOUND)
    pkg_search_module(VS vapoursynth)

    if(VS_FOUND)
        message(STATUS "Found VapourSynth r${VS_VERSION}")

        cmake_path(APPEND install_dir ${VS_LIBDIR} vapoursynth)
        target_include_directories(julek PRIVATE ${VS_INCLUDE_DIRS})

        install(TARGETS julek LIBRARY DESTINATION ${install_dir})
    endif()
endif()

if(NOT VS_FOUND)
    set(VS_INCLUDE_DIR "" CACHE PATH "Path to VapourSynth headers")

    if(VS_INCLUDE_DIR EQUAL "")
        message(WARNING "VapourSynth not found")
    endif()

    target_include_directories(julek PRIVATE ${VS_INCLUDE_DIR})

    install(TARGETS julek LIBRARY RUNTIME)
endif()

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --long --always
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE VCS_TAG
    )
    if(VCS_TAG)
        string(STRIP ${VCS_TAG} VCS_TAG)
    endif()
endif()

if(VCS_TAG)
    message(STATUS "vapoursynth-julek-plugin ${VCS_TAG}")
else()
    message(WARNING "unknown plugin version")
    set(VCS_TAG "unknown")
endif()

configure_file(src/config.h.in config.h)