/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

namespace aac
{

class BitStream;

namespace elements
{

// Program Config Element(
class PCE
{
public:
  PCE() = default;
  virtual ~PCE() = default;

  void Decode(aac::BitStream& stream);

  int GetProfile() const { return m_profile; }
  int GetSampleFrequencyIndex() const { return m_sampleFrequencyIndex; }

private:
  int m_profile = 0;
  int m_sampleFrequencyIndex = 0;
};

} // namespace elements
} // namespace aac
