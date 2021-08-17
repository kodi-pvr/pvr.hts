/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>

namespace aac
{

class BitStream;

namespace elements
{

enum WindowSequence
{
  ONLY_LONG_SEQUENCE,
  LONG_START_SEQUENCE,
  EIGHT_SHORT_SEQUENCE,
  LONG_STOP_SEQUENCE
};

// Individual Channel Stream (ICS) Info
class ICSInfo
{
public:
  ICSInfo() = default;
  virtual ~ICSInfo() = default;

  void Decode(bool commonWindow, aac::BitStream& stream, int profile, int sampleFrequencyIndex);

  int GetMaxSFB() const { return m_maxSFB; }
  int GetWindowGroupCount() const { return m_numWindowGroups; }
  const uint8_t* GetWindowGroupLengths() const { return m_windowGroupLen; }
  WindowSequence GetWindowSequence() const { return m_windowSequence; }
  int GetWindowCount() const { return m_numWindows; }
  const uint16_t* GetSWBOffsets() const { return m_swbOffset; }

  void SetData(const ICSInfo& info);

private:
  void DecodePredictionData(bool commonWindow,
                            BitStream& stream,
                            int profile,
                            int sampleFrequencyIndex);
  void DecodeLTPredictionData(BitStream& stream);

  WindowSequence m_windowSequence = ONLY_LONG_SEQUENCE;
  int m_maxSFB = 0;
  int m_numWindowGroups = 0;
  uint8_t m_windowGroupLen[8] = {};
  const uint16_t* m_swbOffset = nullptr;
  int m_numWindows = 0;
};

} // namespace elements
} // namespace aac
