/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "ICS.h"

#include "../BitStream.h"
#include "../huffman/Decoder.h"
#include "ICSInfo.h"

#include <stdexcept>

using namespace aac;
using namespace aac::elements;

ICS::ICS() : m_icsInfo(new ICSInfo)
{
}

ICS::~ICS() = default;

void ICS::Decode(bool commonWindow, BitStream& stream, int profile, int sampleFrequencyIndex)
{
  // 8 bits global gain
  stream.SkipBits(8);

  if (!commonWindow)
    m_icsInfo->Decode(commonWindow, stream, profile, sampleFrequencyIndex);

  DecodeSectionData(stream);
  DecodeScaleFactorData(stream);

  const bool pulseDataPresent = stream.ReadBool();
  if (pulseDataPresent)
  {
    if (m_icsInfo->GetWindowSequence() == EIGHT_SHORT_SEQUENCE)
      throw std::logic_error(
          "aac::elements::ICS::Decode - Pulse data not allowed for short frames");

    DecodePulseData(stream);
  }

  const bool tnsDataPresent = stream.ReadBool();
  if (tnsDataPresent)
    DecodeTNSData(stream);

  const bool gainControlDataPresent = stream.ReadBool();
  if (gainControlDataPresent)
    DecodeGainControlData(stream);

  DecodeSpectralData(stream);
}

void ICS::DecodeSectionData(BitStream& stream)
{
  const int bits = (m_icsInfo->GetWindowSequence() == EIGHT_SHORT_SEQUENCE) ? 3 : 5;
  const int escVal = (1 << bits) - 1;

  const int windowGroupCount = m_icsInfo->GetWindowGroupCount();
  const int maxSFB = m_icsInfo->GetMaxSFB();

  int idx = 0;
  for (int g = 0; g < windowGroupCount; ++g)
  {
    int k = 0;
    while (k < maxSFB)
    {
      int end = k;
      const int sectCB = stream.ReadBits(4);

      if (sectCB == 12)
        throw std::logic_error(
            "aac::elements::ICS::DecodeSectionData - Invalid huffman codebook: 12");

      int incr;
      while ((incr = stream.ReadBits(bits)) == escVal && stream.GetBitsLeft() >= bits)
        end += incr;

      end += incr;

      if (stream.GetBitsLeft() < 0 || incr == escVal)
        throw std::logic_error("aac::elements::ICS::DecodeSectionData - stream past eof");

      if (end > m_icsInfo->GetMaxSFB())
        throw std::logic_error("aac::elements::ICS::DecodeSectionData - Too many bands");

      for (; k < end; ++k)
      {
        m_sfbCB[idx] = sectCB;
        m_sectEnd[idx++] = end;
      }
    }
  }
}

void ICS::DecodeScaleFactorData(BitStream& stream)
{
  bool noiseFlag = true;

  const int windowGroupCount = m_icsInfo->GetWindowGroupCount();
  const int maxSFB = m_icsInfo->GetMaxSFB();

  int idx = 0;
  for (int g = 0; g < windowGroupCount; ++g)
  {
    for (int sfb = 0; sfb < maxSFB;)
    {
      const int end = m_sectEnd[idx];
      switch (m_sfbCB[idx])
      {
        case huffman::ZERO_HCB:
          for (; sfb < end; ++sfb, ++idx)
            ;
          break;
        case huffman::INTENSITY_HCB:
        case huffman::INTENSITY_HCB2:
          for (; sfb < end; ++sfb, ++idx)
          {
            static constexpr int SF_DELTA = 60;

            if (huffman::Decoder::DecodeScaleFactor(stream) - SF_DELTA > 255)
              throw std::logic_error(
                  "aac::elements::ICS::DecodeScaleFactor - Scalefactor out of range");
          }
          break;
        case huffman::NOISE_HCB:
          for (; sfb < end; ++sfb, ++idx)
          {
            if (noiseFlag)
            {
              stream.SkipBits(9);
              noiseFlag = false;
            }
            else
            {
              huffman::Decoder::DecodeScaleFactor(stream);
            }
          }
          break;
        default:
          for (; sfb < end; ++sfb, ++idx)
          {
            huffman::Decoder::DecodeScaleFactor(stream);
          }
          break;
      }
    }
  }
}

void ICS::DecodePulseData(BitStream& stream)
{
  const int pulseCount = stream.ReadBits(2);

  // 6 bits pulse start sfb
  stream.SkipBits(6);

  //    for (int i = 0; i < pulseCount + 1; ++i)
  //    {
  //      // 5 bits pulse offset, 4 bits pulse amp
  //      m_stream.SkipBits(9);
  //    }
  stream.SkipBits(9 * (pulseCount + 1));
}

void ICS::DecodeTNSData(BitStream& stream)
{
  int bits[3];
  if (m_icsInfo->GetWindowSequence() == EIGHT_SHORT_SEQUENCE)
  {
    bits[0] = 1;
    bits[1] = 4;
    bits[2] = 3;
  }
  else
  {
    bits[0] = 2;
    bits[1] = 6;
    bits[2] = 5;
  }

  const int windowCount = m_icsInfo->GetWindowCount();

  for (int w = 0; w < windowCount; ++w)
  {
    const int nFilt = stream.ReadBits(bits[0]);
    if (nFilt != 0)
    {
      const int coefRes = stream.ReadBit();

      for (int filt = 0; filt < nFilt; ++filt)
      {
        stream.SkipBits(bits[1]);

        const int order = stream.ReadBits(bits[2]);
        if (order != 0)
        {
          // 1 bit direction
          stream.SkipBit();

          const int coefCompress = stream.ReadBit();
          const int coefLen = coefRes + 3 - coefCompress;

          //            for (int i = 0; i < order; ++i)
          //            {
          //              m_stream.SkipBits(coefLen);
          //            }
          stream.SkipBits(order * coefLen);
        }
      }
    }
  }
}

void ICS::DecodeGainControlData(BitStream& stream)
{
  const int maxBand = stream.ReadBits(2) + 1;

  int wdLen, locBits, locBits2 = 0;
  switch (m_icsInfo->GetWindowSequence())
  {
    case ONLY_LONG_SEQUENCE:
      wdLen = 1;
      locBits = 5;
      locBits2 = 5;
      break;
    case EIGHT_SHORT_SEQUENCE:
      wdLen = 8;
      locBits = 2;
      locBits2 = 2;
      break;
    case LONG_START_SEQUENCE:
      wdLen = 2;
      locBits = 4;
      locBits2 = 2;
      break;
    case LONG_STOP_SEQUENCE:
      wdLen = 2;
      locBits = 4;
      locBits2 = 5;
      break;
    default:
      return;
  }

  for (int bd = 1; bd < maxBand; ++bd)
  {
    for (int wd = 0; wd < wdLen; ++wd)
    {
      const int len = stream.ReadBits(3);
      for (int k = 0; k < len; ++k)
      {
        stream.SkipBits(4);
        const int bits = (wd == 0) ? locBits : locBits2;
        stream.SkipBits(bits);
      }
    }
  }
}

void ICS::DecodeSpectralData(BitStream& stream)
{
  const int windowGroupCount = m_icsInfo->GetWindowGroupCount();
  const int maxSFB = m_icsInfo->GetMaxSFB();
  const uint16_t* swbOffsets = m_icsInfo->GetSWBOffsets();

  int idx = 0;
  for (int g = 0; g < windowGroupCount; ++g)
  {
    const int groupLen = m_icsInfo->GetWindowGroupLengths()[g];

    for (int sfb = 0; sfb < maxSFB; ++sfb, ++idx)
    {
      const int hcb = m_sfbCB[idx];
      const int width = swbOffsets[sfb + 1] - swbOffsets[sfb];

      if (hcb == huffman::ZERO_HCB || hcb == huffman::INTENSITY_HCB ||
          hcb == huffman::INTENSITY_HCB2 || hcb == huffman::NOISE_HCB)
      {
        continue;
      }
      else
      {
        for (int w = 0; w < groupLen; ++w)
        {
          static constexpr int FIRST_PAIR_HCB = 5;

          const int num = (hcb >= FIRST_PAIR_HCB) ? 2 : 4;
          for (int k = 0; k < width; k += num)
          {
            int buf[4] = {};
            huffman::Decoder::DecodeSpectralData(stream, hcb, buf, 0);
          }
        }
      }
    }
  }
}
