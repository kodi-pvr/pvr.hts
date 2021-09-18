/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "RDSExtractor.h"

#include "../../aac/Decoder.h"
#include "Logger.h"

#include <exception>

using namespace tvheadend::utilities;

uint8_t RDSExtractorMP2::Decode(const uint8_t* data, size_t len)
{
  Reset();

  const size_t offset = len - 1;
  if (len > 1 && data[offset] == 0xfd)
  {
    m_rdsLen = data[offset - 1];
    if (m_rdsLen > 0)
    {
      m_rdsData = new uint8_t[m_rdsLen];

      // Reassemble UECP block. mpeg stream contains data in reverse order!
      for (size_t i = offset - 2, j = 0; i > 3 && i > offset - 2 - m_rdsLen; i--, j++)
        m_rdsData[j] = data[i];
    }
  }
  return m_rdsLen;
}

uint8_t RDSExtractorAAC::Decode(const uint8_t* data, size_t len)
{
  Reset();

  try
  {
    aac::Decoder decoder(data, len);
    m_rdsLen = decoder.DecodeRDS(m_rdsData);
  }
  catch (std::exception& e)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "AAC RDS extractor exception: %s", e.what());
  }

  return m_rdsLen;
}
