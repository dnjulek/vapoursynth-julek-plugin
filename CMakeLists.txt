cmake_minimum_required(VERSION 3.16)

project(jxlssimulacra LANGUAGES CXX)

###
add_library(ssimulacra_lib STATIC ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libjxl/tools/ssimulacra.cc)

target_include_directories(ssimulacra_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libjxl
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libjxl/jxl_install/include
)

if (NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    endif()

    string(TOLOWER ${CMAKE_BUILD_TYPE} build_type)
    if (build_type STREQUAL debug)
        target_compile_definitions(ssimulacra_lib PRIVATE DEBUG_BUILD)
    else (build_type STREQUAL release)
        target_compile_definitions(ssimulacra_lib PRIVATE RELEASE_BUILD)
    endif()
endif()

target_compile_features(ssimulacra_lib PRIVATE cxx_std_17)

if (UNIX)
    target_compile_options(ssimulacra_lib PRIVATE $<$<CONFIG:RELEASE>:-ffunction-sections -s>)
endif()

find_library(libhwy NAMES hwy PATHS ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libjxl/jxl_install/lib)
message(STATUS "ssimulacra_lib: find_library returned ${libhwy}")


###
project(jxlssimulacra2 LANGUAGES CXX)

add_library(ssimulacra2_lib STATIC ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libjxl/tools/ssimulacra2.cc)

target_include_directories(ssimulacra2_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libjxl
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libjxl/jxl_install/include
)

if (NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    endif()

    string(TOLOWER ${CMAKE_BUILD_TYPE} build_type)
    if (build_type STREQUAL debug)
        target_compile_definitions(ssimulacra2_lib PRIVATE DEBUG_BUILD)
    else (build_type STREQUAL release)
        target_compile_definitions(ssimulacra2_lib PRIVATE RELEASE_BUILD)
    endif()
endif()

target_compile_features(ssimulacra2_lib PRIVATE cxx_std_17)

if (UNIX)
    target_compile_options(ssimulacra2_lib PRIVATE $<$<CONFIG:RELEASE>:-ffunction-sections -s>)
endif()

find_library(libhwy NAMES hwy PATHS ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libjxl/jxl_install/lib)
message(STATUS "ssimulacra2_lib: find_library returned ${libhwy}")


###
project(libjulek LANGUAGES CXX)

add_library(julek SHARED
    src/shared.cpp
    src/Butteraugli.cpp
    src/RFS.cpp
    src/ssimulacra.cpp)

target_include_directories(julek PRIVATE
    thirdparty/libjxl
    thirdparty/libjxl/jxl_install/include)

set_target_properties(julek PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)

find_library(libjxl NAMES
    jxl
    jxl-static
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libjxl/jxl_install/lib
)
message(STATUS "julek: find_library returned ${libjxl}")

find_library(libhwy NAMES hwy PATHS ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libjxl/jxl_install/lib)
message(STATUS "julek: find_library returned ${libhwy}")

target_link_libraries(julek PRIVATE
    ${libjxl}
    ${libhwy}
)

if (WIN32)
    if (NOT CMAKE_GENERATOR MATCHES "Visual Studio")
        target_link_libraries(julek PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}/ssimulacra_lib.lib
            ${CMAKE_CURRENT_BINARY_DIR}/ssimulacra2_lib.lib
        )
    else()
        target_link_libraries(julek PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/ssimulacra_lib.lib
            ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/ssimulacra2_lib.lib
        )
    endif()
else()
    target_link_libraries(julek PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}/libssimulacra_lib.a
            ${CMAKE_CURRENT_BINARY_DIR}/libssimulacra2_lib.a
    )
endif()

add_dependencies(julek ssimulacra_lib ssimulacra2_lib)

find_package(PkgConfig QUIET MODULE)

if(PKG_CONFIG_FOUND)
    pkg_search_module(VS vapoursynth)

    if(VS_FOUND)
        message(STATUS "Found VapourSynth r${VS_VERSION}")
        target_include_directories(julek PRIVATE ${VS_INCLUDE_DIRS})
    endif()
endif()

if(NOT VS_FOUND)
    set(VS_INCLUDE_DIR "" CACHE PATH "Path to VapourSynth headers")

    if(VS_INCLUDE_DIR STREQUAL "")
        message(WARNING "VapourSynth not found")
    endif()

    target_include_directories(julek PRIVATE ${VS_INCLUDE_DIR})
endif()

if(VS_FOUND)
    cmake_path(APPEND install_dir ${VS_LIBDIR} vapoursynth)
    install(TARGETS julek LIBRARY DESTINATION ${install_dir})
else()
    if(WIN32)
        install(TARGETS julek RUNTIME)
    else()
        install(TARGETS julek LIBRARY)
    endif()
endif()