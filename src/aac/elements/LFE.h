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

// Low Frequency Enhancement (LFE) Channel Element
class LFE
{
public:
  LFE() = default;
  virtual ~LFE() = default;

  void Decode(aac::BitStream& stream, int profile, int sampleFrequencyIndex);
};

} // namespace elements
} // namespace aac
