/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */
#ifndef XBMC_CODEC_DESCRIPTOR_HPP
#define	XBMC_CODEC_DESCRIPTOR_HPP

#include "kodi/libXBMC_codec.h"

/**
 * Adapter which converts codec names used by tvheadend and VDR into their 
 * FFmpeg equivalents.
 */
class CodecDescriptor
{
public:
  CodecDescriptor(void)
  {
    m_codec.codec_id   = XBMC_INVALID_CODEC_ID;
    m_codec.codec_type = XBMC_CODEC_TYPE_UNKNOWN;
  }

  CodecDescriptor(xbmc_codec_t codec, const char* name) :
    m_codec(codec),
    m_strName(name) {}
  virtual ~CodecDescriptor(void) {}

  xbmc_codec_t Codec(void) const { return m_codec; }

  static CodecDescriptor GetCodecByName(const char* strCodecName)
  {
    CodecDescriptor retVal;
    // some of Tvheadend's and VDR's codec names don't match ffmpeg's, so translate them to something ffmpeg understands
    if (!strcmp(strCodecName, "MPEG2AUDIO"))
      retVal = CodecDescriptor(CODEC->GetCodecByName("MP2"), strCodecName);
    else if (!strcmp(strCodecName, "MPEGTS"))
      retVal = CodecDescriptor(CODEC->GetCodecByName("MPEG2VIDEO"), strCodecName);
    else if (!strcmp(strCodecName, "TEXTSUB"))
      retVal = CodecDescriptor(CODEC->GetCodecByName("TEXT"), strCodecName);
    else
      retVal = CodecDescriptor(CODEC->GetCodecByName(strCodecName), strCodecName);

    return retVal;
  }

private:
  xbmc_codec_t m_codec;
  std::string  m_strName;
};

#endif	/* XBMC_CODEC_DESCRIPTOR_HPP */