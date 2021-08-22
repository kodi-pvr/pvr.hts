/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "BitStream.h"
#include "Profile.h"
#include "SampleFrequency.h"

#include <cstdint>

namespace aac
{

class Decoder
{
public:
  Decoder() = delete;
  Decoder(const uint8_t* const data, unsigned int dataLen) : m_stream(data, dataLen) {}

  // Note: The only purpose of this decoder currently is decoding RDS data.
  uint8_t DecodeRDS(uint8_t*& data);

private:
  void DecodeFrame();
  void DecodeADTSHeader();
  void DecodeRawDataBlock();

  BitStream m_stream;
  int m_profile = PROFILE_UNKNOWN;
  int m_sampleFrequencyIndex = SAMPLE_FREQUENCY_NONE;
  int m_rawDataBlockCount = 0;

  bool m_rdsOnly = false;
  uint8_t* m_rdsData = nullptr;
  uint8_t m_rdsDataLen = 0;
};

} // namespace aac
