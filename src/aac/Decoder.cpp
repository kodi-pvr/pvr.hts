/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Decoder.h"

#include "elements/CCE.h"
#include "elements/CPE.h"
#include "elements/DSE.h"
#include "elements/FIL.h"
#include "elements/LFE.h"
#include "elements/PCE.h"
#include "elements/SCE.h"

#include <stdexcept>

using namespace aac;

void Decoder::DecodeFrame()
{
  DecodeADTSHeader();

  for (int i = 0; i < m_rawDataBlockCount; ++i)
    DecodeRawDataBlock();
}

uint8_t Decoder::DecodeRDS(uint8_t*& data)
{
  m_rdsOnly = true;

  DecodeFrame();
  if (m_rdsDataLen)
    data = m_rdsData;

  return m_rdsDataLen;
}

void Decoder::DecodeADTSHeader()
{
  // Fixed header

  // 12 bits syncword
  if (m_stream.ReadBits(12) != 0xFFF)
    throw std::logic_error("aac::Decoder::DecodeADTSHeader - Invalid ADTS syncword");

  // 1 bit ID, 2 bits layer
  m_stream.SkipBits(3);

  // 1 bit protection absent
  const bool protectionAbsent = m_stream.ReadBool();

  m_profile = m_stream.ReadBits(2);

  m_sampleFrequencyIndex = m_stream.ReadBits(4);

  // 1 bit private bit, 3 bits channel configuration, 1 bit copy, 1 bit home
  m_stream.SkipBits(6);

  // Variable header

  // 1 bit copyrightIDBit, 1 bit copyrightIDStart
  m_stream.SkipBits(2);

  // 13 bits frame length
  const int aacFrameLength = m_stream.ReadBits(13);
  if (aacFrameLength != m_stream.GetLength())
    throw std::logic_error("aac::Decoder::DecodeADTSHeader - Invalid ADTS frame length");

  // 11 bits adtsBufferFullness
  m_stream.SkipBits(11);

  m_rawDataBlockCount = m_stream.ReadBits(2) + 1;

  if (!protectionAbsent)
  {
    // 16 bits CRC data
    m_stream.SkipBits(16);
  }
}

namespace
{

constexpr int ELEMENT_SCE = 0;
constexpr int ELEMENT_CPE = 1;
constexpr int ELEMENT_CCE = 2;
constexpr int ELEMENT_LFE = 3;
constexpr int ELEMENT_DSE = 4;
constexpr int ELEMENT_PCE = 5;
constexpr int ELEMENT_FIL = 6;
constexpr int ELEMENT_END = 7;

} // unnamed namespace

void Decoder::DecodeRawDataBlock()
{
  int type = ELEMENT_END;

  do
  {
    // 3 bits elem type
    type = m_stream.ReadBits(3);

    switch (type)
    {
      case ELEMENT_SCE:
      {
        elements::SCE sce;
        sce.Decode(m_stream, m_profile, m_sampleFrequencyIndex);
        break;
      }
      case ELEMENT_LFE:
      {
        elements::LFE lfe;
        lfe.Decode(m_stream, m_profile, m_sampleFrequencyIndex);
        break;
      }
      case ELEMENT_CPE:
      {
        elements::CPE cpe;
        cpe.Decode(m_stream, m_profile, m_sampleFrequencyIndex);
        break;
      }
      case ELEMENT_CCE:
      {
        elements::CCE cce;
        cce.Decode(m_stream, m_profile, m_sampleFrequencyIndex);
        break;
      }
      case ELEMENT_DSE:
      {
        elements::DSE dse;
        if (m_rdsOnly)
        {
          //! @todo can there be more than one rds data set in one frame?
          m_rdsDataLen = dse.DecodeRDS(m_stream, m_rdsData);
        }
        else
        {
          dse.Decode(m_stream);
        }
        break;
      }
      case ELEMENT_PCE:
      {
        elements::PCE pce;
        pce.Decode(m_stream);
        m_profile = pce.GetProfile();
        m_sampleFrequencyIndex = pce.GetSampleFrequencyIndex();
        break;
      }
      case ELEMENT_FIL:
      {
        elements::FIL fil;
        fil.Decode(m_stream);
        break;
      }
      case ELEMENT_END:
        break;

      default:
        throw std::logic_error("aac::Decoder::DecodeRawDataBlock - Unexpected element type");
    }
  } while (type != ELEMENT_END);

  m_stream.ByteAlign();
}
