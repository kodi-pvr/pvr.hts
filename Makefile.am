#
# Makefile for the HTS Tvheadend add-on for XBMC PVR
#
# See the README for copyright information and
# how to reach the author.
#

ADDONBINNAME    = XBMC_Tvheadend
ADDONNAME       = pvr.hts
LIBNAME         = libtvheadend-addon
lib_LTLIBRARIES = libtvheadend-addon.la

LIBS            = @abs_top_srcdir@/lib/libhts/libhts.la -ldl

include ../Makefile.include.am

libtvheadend_addon_la_SOURCES = src/client.cpp \
                                src/HTSPConnection.cpp \
                                src/HTSPData.cpp \
                                src/HTSPDemux.cpp
libtvheadend_addon_la_LDFLAGS = @TARGET_LDFLAGS@

