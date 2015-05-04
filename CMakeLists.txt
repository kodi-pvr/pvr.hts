project(pvr.hts)

cmake_minimum_required(VERSION 2.6)

enable_language(CXX)

find_package(kodi REQUIRED)
find_package(kodiplatform REQUIRED)

include_directories(${kodiplatform_INCLUDE_DIRS}
                    ${KODI_INCLUDE_DIR}
                    ${PROJECT_SOURCE_DIR}/lib)

add_definitions(-DUSE_DEMUX)

set(HTS_SOURCES src/AsyncState.cpp
                src/AsyncState.h
                src/CircBuffer.cpp
                src/CircBuffer.h
                src/client.h
                src/client.cpp
                src/HTSPConnection.cpp
                src/HTSPDemuxer.cpp
                src/HTSPTypes.h
                src/HTSPTypes.cpp
                src/HTSPVFS.cpp
                src/Settings.h
                src/Tvheadend.cpp
                src/Tvheadend.h
                src/xbmc_codec_descriptor.hpp)

add_subdirectory(lib/libhts)

set(DEPLIBS ${kodiplatform_LIBRARIES} hts)
if(WIN32)
  list(APPEND DEPLIBS ws2_32)
endif()

build_addon(pvr.hts HTS DEPLIBS)

include(CPack)
