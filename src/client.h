#pragma once
/*
 *      Copyright (C) 2005-2011 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "kodi/os.h"
#include "kodi/threads/mutex.h"
#include "kodi/libXBMC_addon.h"
#include "kodi/libXBMC_pvr.h"
#include "kodi/libXBMC_gui.h"
#include "kodi/libXBMC_codec.h"

extern ADDON::CHelper_libXBMC_addon*  XBMC;
extern CHelper_libXBMC_pvr*           PVR;
extern CHelper_libXBMC_gui*           GUI;
extern CHelper_libXBMC_codec*         CODEC;

#define DEFAULT_HOST             "127.0.0.1"
#define DEFAULT_HTTP_PORT        9981
#define DEFAULT_HTSP_PORT        9982
#define DEFAULT_CONNECT_TIMEOUT  10
#define DEFAULT_RESPONSE_TIMEOUT 5

class CTvheadend;
extern CTvheadend                *tvh;
