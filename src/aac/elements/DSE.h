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

// Data Stream Element
class DSE
{
public:
  DSE() = default;
  virtual ~DSE() = default;

  void Decode(aac::BitStream& stream);
  uint8_t DecodeRDS(aac::BitStream& stream, uint8_t*& rdsdata);
};

} // namespace elements
} // namespace aac
