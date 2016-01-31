[![Build Status](https://travis-ci.org/kodi-pvr/pvr.hts.svg?branch=master)](https://travis-ci.org/kodi-pvr/pvr.hts)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/5120/badge.svg)](https://scan.coverity.com/projects/5120)

# Tvheadend PVR addon for Kodi

This is a [Kodi] (http://kodi.tv) PVR addon for connecting to a [tvheadend](https://tvheadend.org) backend.

## Build instructions

When building the addon you have to use the correct branch depending on which version of Kodi you're building against. 
For example, if you're building the `Jarvis` branch of Kodi you should checkout the `Jarvis` branch of this repository. 
Addon releases are also tagged regularly.

### Linux

1. `git clone https://github.com/xbmc/xbmc.git`
2. `git clone https://github.com/kodi-pvr/pvr.hts.git`
3. `cd pvr.hts && mkdir build && cd build`
4. `cmake -DADDONS_TO_BUILD=pvr.hts -DADDON_SRC_PREFIX=../.. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../../xbmc/addons -DPACKAGE_ZIP=1 ../../xbmc/project/cmake/addons`
5. `make`

The addon files will be placed in `../../xbmc/addons` so if you build Kodi from source and run it directly 
the addon will be available as a system addon.

##### Useful links

* [Kodi's PVR user support] (http://forum.kodi.tv/forumdisplay.php?fid=167)
* [Kodi's PVR development support] (http://forum.kodi.tv/forumdisplay.php?fid=136)
