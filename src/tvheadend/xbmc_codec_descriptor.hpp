/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "kodi/libXBMC_pvr.h"

#include <cstring>

/**
 * Adapter which converts codec names used by tvheadend and VDR into their 
 * FFmpeg equivalents.
 */
class CodecDescriptor
{
public:
  CodecDescriptor()
  {
    m_codec.codec_id = XBMC_INVALID_CODEC_ID;
    m_codec.codec_type = XBMC_CODEC_TYPE_UNKNOWN;
  }

  CodecDescriptor(xbmc_codec_t codec, const char* name) : m_codec(codec), m_strName(name) {}
  virtual ~CodecDescriptor() {}

  xbmc_codec_t Codec() const { return m_codec; }

  static CodecDescriptor GetCodecByName(const char* strCodecName)
  {
    CodecDescriptor retVal;
    // some of Tvheadend's and VDR's codec names don't match ffmpeg's, so translate them to something ffmpeg understands
    if (!std::strcmp(strCodecName, "MPEG2AUDIO"))
      retVal = CodecDescriptor(PVR->GetCodecByName("MP2"), strCodecName);
    else if (!std::strcmp(strCodecName, "MPEGTS"))
      retVal = CodecDescriptor(PVR->GetCodecByName("MPEG2VIDEO"), strCodecName);
    else if (!std::strcmp(strCodecName, "TEXTSUB"))
      retVal = CodecDescriptor(PVR->GetCodecByName("TEXT"), strCodecName);
    else
      retVal = CodecDescriptor(PVR->GetCodecByName(strCodecName), strCodecName);

    return retVal;
  }

private:
  xbmc_codec_t m_codec;
  std::string m_strName;
};
