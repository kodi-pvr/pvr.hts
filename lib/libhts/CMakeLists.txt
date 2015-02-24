PROJECT(hts)

ENABLE_LANGUAGE(C)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(SOURCES htsbuf.c
            htsmsg_binary.c
            htsmsg.c
            htsstr.c
            sha1.c)
if(WIN32)
  LIST(APPEND SOURCES net_winsock.c)

  include_directories("${PROJECT_SOURCE_DIR}/Win32/include")
else()
  LIST(APPEND SOURCES net_posix.c)

  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -fPIC")
endif()

ADD_DEFINITIONS(-D_GNU_SOURCE)

ADD_LIBRARY(hts STATIC ${SOURCES})
