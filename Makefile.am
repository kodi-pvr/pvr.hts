#
# Makefile for the HTS Tvheadend add-on for XBMC PVR
#
# See the README for copyright information and
# how to reach the author.
#

ADDONBINNAME    = Kodi_Tvheadend
ADDONNAME       = pvr.hts
LIBNAME         = libtvheadend-addon
lib_LTLIBRARIES = libtvheadend-addon.la

LIBS            = @abs_top_srcdir@/lib/libhts/libhts.la -ldl

include ../Makefile.include.am

libtvheadend_addon_la_SOURCES = src/client.cpp \
                               src/AsyncState.cpp \
                               src/Tvheadend.cpp \
                               src/HTSPConnection.cpp \
                               src/HTSPDemuxer.cpp \
                               src/HTSPVFS.cpp \
                               src/CircBuffer.cpp
libtvheadend_addon_la_LDFLAGS = @TARGET_LDFLAGS@

