/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "DSE.h"

#include "../BitStream.h"

#include <cstring>
#include <exception>

using namespace aac;
using namespace aac::elements;

void DSE::Decode(BitStream& stream)
{
  // 4 bits elem id
  stream.SkipBits(4);

  const bool byteAlign = stream.ReadBool();
  int count = stream.ReadBits(8);
  if (count == 255)
    count += stream.ReadBits(8);

  if (byteAlign)
    stream.ByteAlign();

  stream.SkipBits(8 * count);
}

uint8_t DSE::DecodeRDS(BitStream& stream, uint8_t*& rdsdata)
{
  // 4 bits elem id
  stream.SkipBits(4);

  const bool byteAlign = stream.ReadBool();
  int count = stream.ReadBits(8);
  if (count == 255)
    count += stream.ReadBits(8);

  if (byteAlign)
    stream.ByteAlign();

  static constexpr int BUFFER_SIZE = 65536;
  static uint8_t buffer[BUFFER_SIZE];
  static int bufferpos = 0;

  uint8_t ret = 0;

  if (count > BUFFER_SIZE)
  {
    // data package too large! turn over with next package.
    stream.SkipBits(8 * count);
    bufferpos = 0;
    return ret;
  }

  if (bufferpos + count > BUFFER_SIZE)
  {
    // buffer overflow! turn over now.
    bufferpos = 0;
  }

  try
  {
    // collect data
    for (int i = 0; i < count; ++i)
    {
      buffer[bufferpos + i] = static_cast<uint8_t>(stream.ReadBits(8));
    }
    bufferpos += count;
  }
  catch (std::exception&)
  {
    // cleanup and rethrow
    bufferpos = 0;
    throw;
  }

  if (bufferpos > 0 && buffer[bufferpos - 1] == 0xFF)
  {
    if (buffer[0] == 0xFE)
    {
      // data package is complete. deliver it.
      rdsdata = new uint8_t[bufferpos]; // Note: caller has to delete the data array
      std::memcpy(rdsdata, buffer, bufferpos);
      ret = bufferpos;
    }
    bufferpos = 0;
  }

  return ret;
}
