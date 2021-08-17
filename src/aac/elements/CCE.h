/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  https://kodi.tv
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

// Coupling Channel Element
class CCE
{
public:
  CCE() = default;
  virtual ~CCE() = default;

  void Decode(aac::BitStream& stream, int profile, int sampleFrequencyIndex);
};

} // namespace elements
} // namespace aac
