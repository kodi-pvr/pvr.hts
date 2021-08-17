/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "ICSInfo.h"

#include "../BitStream.h"
#include "../Profile.h"
#include "../SampleFrequency.h"

#include <algorithm>
#include <stdexcept>

using namespace aac;
using namespace aac::elements;

namespace
{

constexpr int PRED_SFB_MAX[] = {33, 33, 38, 40, 40, 40, 41, 41, 37, 37, 37, 34};

constexpr uint16_t SWB_OFFSET_128_96[] = {0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 92, 128};

constexpr uint16_t SWB_OFFSET_128_64[] = {0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 92, 128};

constexpr uint16_t SWB_OFFSET_128_48[] = {0,  4,  8,  12, 16, 20,  28, 36,
                                          44, 56, 68, 80, 96, 112, 128};

constexpr uint16_t SWB_OFFSET_128_24[] = {0,  4,  8,  12, 16, 20, 24,  28,
                                          36, 44, 52, 64, 76, 92, 108, 128};

constexpr uint16_t SWB_OFFSET_128_16[] = {0,  4,  8,  12, 16, 20, 24,  28,
                                          32, 40, 48, 60, 72, 88, 108, 128};

constexpr uint16_t SWB_OFFSET_128_8[] = {0,  4,  8,  12, 16, 20, 24,  28,
                                         36, 44, 52, 60, 72, 88, 108, 128};

const uint16_t* const SWB_OFFSET_128[] = {SWB_OFFSET_128_96, SWB_OFFSET_128_96, SWB_OFFSET_128_64,
                                          SWB_OFFSET_128_48, SWB_OFFSET_128_48, SWB_OFFSET_128_48,
                                          SWB_OFFSET_128_24, SWB_OFFSET_128_24, SWB_OFFSET_128_16,
                                          SWB_OFFSET_128_16, SWB_OFFSET_128_16, SWB_OFFSET_128_8};

constexpr uint16_t SWB_OFFSET_1024_96[] = {0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,
                                           44,  48,  52,  56,  64,  72,  80,  88,  96,  108, 120,
                                           132, 144, 156, 172, 188, 212, 240, 276, 320, 384, 448,
                                           512, 576, 640, 704, 768, 832, 896, 960, 1024};

constexpr uint16_t SWB_OFFSET_1024_64[] = {
    0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  44,  48,  52,  56,  64,
    72,  80,  88,  100, 112, 124, 140, 156, 172, 192, 216, 240, 268, 304, 344, 384,
    424, 464, 504, 544, 584, 624, 664, 704, 744, 784, 824, 864, 904, 944, 984, 1024};

constexpr uint16_t SWB_OFFSET_1024_48[] = {
    0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  48,  56,  64,  72,  80,  88,
    96,  108, 120, 132, 144, 160, 176, 196, 216, 240, 264, 292, 320, 352, 384, 416, 448,
    480, 512, 544, 576, 608, 640, 672, 704, 736, 768, 800, 832, 864, 896, 928, 1024};

constexpr uint16_t SWB_OFFSET_1024_32[] = {
    0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  48,  56,  64,  72,  80,  88,  96,
    108, 120, 132, 144, 160, 176, 196, 216, 240, 264, 292, 320, 352, 384, 416, 448, 480, 512,
    544, 576, 608, 640, 672, 704, 736, 768, 800, 832, 864, 896, 928, 960, 992, 1024};

constexpr uint16_t SWB_OFFSET_1024_24[] = {
    0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  44,  52,  60,  68,  76,
    84,  92,  100, 108, 116, 124, 136, 148, 160, 172, 188, 204, 220, 240, 260, 284,
    308, 336, 364, 396, 432, 468, 508, 552, 600, 652, 704, 768, 832, 896, 960, 1024};

constexpr uint16_t SWB_OFFSET_1024_16[] = {0,   8,   16,  24,  32,  40,  48,  56,  64,  72,  80,
                                           88,  100, 112, 124, 136, 148, 160, 172, 184, 196, 212,
                                           228, 244, 260, 280, 300, 320, 344, 368, 396, 424, 456,
                                           492, 532, 572, 616, 664, 716, 772, 832, 896, 960, 1024};

constexpr uint16_t SWB_OFFSET_1024_8[] = {0,   12,  24,  36,  48,  60,  72,  84,  96,  108, 120,
                                          132, 144, 156, 172, 188, 204, 220, 236, 252, 268, 288,
                                          308, 328, 348, 372, 396, 420, 448, 476, 508, 544, 580,
                                          620, 664, 712, 764, 820, 880, 944, 1024};

const uint16_t* const SWB_OFFSET_1024[] = {
    SWB_OFFSET_1024_96, SWB_OFFSET_1024_96, SWB_OFFSET_1024_64, SWB_OFFSET_1024_48,
    SWB_OFFSET_1024_48, SWB_OFFSET_1024_32, SWB_OFFSET_1024_24, SWB_OFFSET_1024_24,
    SWB_OFFSET_1024_16, SWB_OFFSET_1024_16, SWB_OFFSET_1024_16, SWB_OFFSET_1024_8};

} // unnamed namespace

void ICSInfo::SetData(const ICSInfo& info)
{
  m_windowSequence = info.m_windowSequence;
  m_maxSFB = info.m_maxSFB;
  m_numWindowGroups = info.m_numWindowGroups;
  for (int i = 0; i < 8; ++i)
    m_windowGroupLen[i] = info.m_windowGroupLen[i];
  m_swbOffset = info.m_swbOffset;
  m_numWindows = info.m_numWindows;
}

void ICSInfo::Decode(bool commonWindow, BitStream& stream, int profile, int sampleFrequencyIndex)
{
  if (sampleFrequencyIndex == SAMPLE_FREQUENCY_NONE)
    throw std::invalid_argument("aac::elements::ICSInfo::Decode - Invalid sample frequency");

  // 1 bit reserved
  stream.SkipBit();

  const int ws = stream.ReadBits(2);
  switch (ws)
  {
    case 0:
      m_windowSequence = ONLY_LONG_SEQUENCE;
      break;
    case 1:
      m_windowSequence = LONG_START_SEQUENCE;
      break;
    case 2:
      m_windowSequence = EIGHT_SHORT_SEQUENCE;
      break;
    case 3:
      m_windowSequence = LONG_STOP_SEQUENCE;
      break;
    default:
      throw std::logic_error("aac::elements::ICSInfo::Decode - Invalid window sequence");
  }

  // 1 bit window shape
  stream.SkipBit();

  m_numWindowGroups = 1;
  m_windowGroupLen[0] = 1;

  if (m_windowSequence == EIGHT_SHORT_SEQUENCE)
  {
    m_maxSFB = stream.ReadBits(4);

    // 7 bits scale factor grouping
    for (int i = 0; i < 7; ++i)
    {
      if (stream.ReadBool())
      {
        m_windowGroupLen[m_numWindowGroups - 1]++;
      }
      else
      {
        m_numWindowGroups++;
        m_windowGroupLen[m_numWindowGroups - 1] = 1;
      }
    }

    m_numWindows = 8;
    m_swbOffset = SWB_OFFSET_128[sampleFrequencyIndex];
  }
  else
  {
    m_maxSFB = stream.ReadBits(6);

    m_numWindows = 1;
    m_swbOffset = SWB_OFFSET_1024[sampleFrequencyIndex];

    const bool predictorDataPresent = stream.ReadBool();
    if (predictorDataPresent)
      DecodePredictionData(commonWindow, stream, profile, sampleFrequencyIndex);
  }
}

void ICSInfo::DecodePredictionData(bool commonWindow,
                                   BitStream& stream,
                                   int profile,
                                   int sampleFrequencyIndex)
{
  switch (profile)
  {
    case PROFILE_AAC_MAIN:
    {
      const bool predictorReset = stream.ReadBool();
      if (predictorReset)
      {
        // 5 bits predictor reset group number
        stream.SkipBits(5);
      }

      const int maxPredSFB = PRED_SFB_MAX[sampleFrequencyIndex];
      const int length = std::min(m_maxSFB, maxPredSFB);

      //        for (int sfb = 0; sfb < length; ++sfb)
      //        {
      //          // 1 bit prediction used
      //          m_stream.SkipBit();
      //        }
      stream.SkipBits(length);
      break;
    }

    case PROFILE_AAC_LTP:
    {
      const bool ltpData1Present = stream.ReadBool();
      if (ltpData1Present)
        DecodeLTPredictionData(stream);

      if (commonWindow)
      {
        const bool ltpData2Present = stream.ReadBool();
        if (ltpData2Present)
          DecodeLTPredictionData(stream);
      }
      break;
    }

    case PROFILE_ER_AAC_LTP:
    {
      if (!commonWindow)
      {
        const bool ltpData1Present = stream.ReadBool();
        if (ltpData1Present)
          DecodeLTPredictionData(stream);
      }
      break;
    }
    default:
      throw std::logic_error(
          "aac::elements::ICSInfo::DecodePredictionData - Unexpected profile for LTP");
  }
}

void ICSInfo::DecodeLTPredictionData(BitStream& stream)
{
  // 11 bits lag, 3 bits coef
  stream.SkipBits(14);

  if (m_windowSequence == EIGHT_SHORT_SEQUENCE)
  {
    for (int w = 0; w < m_numWindows; ++w)
    {
      const bool shortUsed = stream.ReadBool();
      if (shortUsed)
      {
        const bool shortLagPresent = stream.ReadBool();
        if (shortLagPresent)
        {
          // 4 bits short lag
          stream.SkipBits(4);
        }
      }
    }
  }
  else
  {
    static constexpr int MAX_LTP_SFB = 40;
    const int lastBand = std::min(m_maxSFB, MAX_LTP_SFB);
    //          for (int i = 0; i < lastBand; ++i)
    //          {
    //            // 1 bit long used
    //            m_stream.SkipBit();
    //          }
    stream.SkipBits(lastBand);
  }
}
