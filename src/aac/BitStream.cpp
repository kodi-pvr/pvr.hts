/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "BitStream.h"

#include <stdexcept>

using namespace aac;

BitStream::BitStream(const uint8_t* data, unsigned int dataLen) : m_data(data), m_dataLen(dataLen)
{
}

int BitStream::GetBitsLeft() const
{
  return 8 * (m_dataLen - m_pos) + m_bitsCached;
}

int BitStream::ReadBit()
{
  int result;

  if (m_bitsCached > 0)
  {
    m_bitsCached--;
  }
  else
  {
    m_cache = ReadCache();
    m_bitsCached = 31;
  }

  result = (m_cache >> m_bitsCached) & 0x1;
  m_bitsRead++;

  return result;
}

int BitStream::ReadBits(int n)
{
  if (n > 32)
    throw std::invalid_argument("aac::BitStream::ReadBits - Attempt to read more than 32 bits");

  int result;

  if (m_bitsCached >= n)
  {
    m_bitsCached -= n;
    result = (m_cache >> m_bitsCached) & MaskBits(n);
  }
  else
  {
    const uint32_t c = m_cache & MaskBits(m_bitsCached);
    const int left = n - m_bitsCached;

    m_cache = ReadCache();
    m_bitsCached = 32 - left;
    result = ((m_cache >> m_bitsCached) & MaskBits(left)) | (c << left);
  }

  m_bitsRead += n;
  return result;
}

void BitStream::SkipBit()
{
  m_bitsRead++;
  if (m_bitsCached > 0)
  {
    m_bitsCached--;
  }
  else
  {
    m_cache = ReadCache();
    m_bitsCached = 31;
  }
}

void BitStream::SkipBits(int n)
{
  m_bitsRead += n;
  if (n <= m_bitsCached)
  {
    m_bitsCached -= n;
  }
  else
  {
    n -= m_bitsCached;

    while (n >= 32)
    {
      n -= 32;
      ReadCache();
    }

    if (n > 0)
    {
      m_cache = ReadCache();
      m_bitsCached = 32 - n;
    }
    else
    {
      m_cache = 0;
      m_bitsCached = 0;
    }
  }
}

void BitStream::ByteAlign()
{
  const int toFlush = m_bitsCached & 0x7;
  if (toFlush > 0)
    SkipBits(toFlush);
}

uint32_t BitStream::ReadCache()
{
  if (m_pos == m_dataLen)
  {
    throw std::out_of_range("aac::BitStream::ReadCache - Attempt to read past end of stream");
  }
  else if (m_pos > m_dataLen - 4)
  {
    // read near end of stream; read last 1 to 3 bytes
    int toRead = m_dataLen - m_pos;
    int i = 0;
    if (toRead-- > 0)
      i = ((m_data[m_pos] & 0xFF) << 24);
    if (toRead-- > 0)
      i |= ((m_data[m_pos + 1] & 0xFF) << 16);
    if (toRead-- > 0)
      i |= ((m_data[m_pos + 2] & 0xFF) << 8);

    m_pos = m_dataLen;
    return i;
  }
  else
  {
    // read next 4 bytes
    const uint32_t i = ((m_data[m_pos] & 0xFF) << 24) | ((m_data[m_pos + 1] & 0xFF) << 16) |
                       ((m_data[m_pos + 2] & 0xFF) << 8) | (m_data[m_pos + 3] & 0xFF);

    m_pos += 4;
    return i;
  }
}

int BitStream::MaskBits(int n)
{
  return (n == 32) ? -1 : (1 << n) - 1;
}
