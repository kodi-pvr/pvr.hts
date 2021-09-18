/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>

namespace aac
{

class BitStream
{
public:
  BitStream() = delete;
  BitStream(const uint8_t* data, unsigned int dataLen);

  unsigned int GetLength() const { return m_dataLen; }

  int GetBitsLeft() const;

  int ReadBit();
  int ReadBits(int n);

  bool ReadBool() { return (ReadBit() & 0x1) != 0; }

  void SkipBit();
  void SkipBits(int n);

  void ByteAlign();

private:
  uint32_t ReadCache();
  int MaskBits(int n);

  const uint8_t* m_data = nullptr;
  const unsigned int m_dataLen = 0;

  unsigned int m_pos = 0; // offset in the data array
  uint32_t m_cache = 0; // current 4 bytes, that are read from data
  unsigned int m_bitsCached = 0; // remaining bits in current cache
  unsigned int m_bitsRead = 0; // number of total bits read
};

} // namespace aac
