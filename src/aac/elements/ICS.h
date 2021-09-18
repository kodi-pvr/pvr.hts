/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>
#include <memory>

namespace aac
{

class BitStream;

namespace elements
{

class ICSInfo;

// Individual Channel Stream
class ICS
{
public:
  ICS();
  virtual ~ICS();

  void Decode(bool commonWindow, aac::BitStream& stream, int profile, int sampleFrequencyIndex);

  ICSInfo& GetInfo() const { return *m_icsInfo; }
  int const* GetSfbCB() const { return m_sfbCB; }

private:
  void DecodeSectionData(BitStream& stream);
  void DecodeScaleFactorData(BitStream& stream);
  void DecodePulseData(BitStream& stream);
  void DecodeTNSData(BitStream& stream);
  void DecodeGainControlData(BitStream& stream);
  void DecodeSpectralData(BitStream& stream);

  std::unique_ptr<ICSInfo> m_icsInfo;

  int m_sfbCB[120] = {};
  int m_sectEnd[120] = {};
};

} // namespace elements
} // namespace aac
