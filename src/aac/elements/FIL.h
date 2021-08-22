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

// Fill Element
class FIL
{
public:
  FIL() = default;
  virtual ~FIL() = default;

  void Decode(aac::BitStream& stream);
};

} // namespace elements
} // namespace aac
