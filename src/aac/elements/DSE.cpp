/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "DSE.h"

#include "../BitStream.h"

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

  if (count > 2) // we need at least 0xFE + some-other-byte + 0xFF
  {
    const uint8_t firstElem = static_cast<uint8_t>(stream.ReadBits(8));
    if (firstElem == 0xFE) // could be RDS data start
    {
      rdsdata = new uint8_t[count];
      rdsdata[0] = firstElem;

      try
      {
        for (int i = 1; i < count; ++i)
          rdsdata[i] = static_cast<uint8_t>(stream.ReadBits(8));

        if (rdsdata[count - 1] == 0xFF) // RDS data end
          return count; // Note: caller has to delete the data array
      }
      catch (std::exception&)
      {
        // cleanup and rethrow
        delete[] rdsdata;
        rdsdata = nullptr;
        throw;
      }

      // data start with 0xFE, but do not end with 0xFF, thus no RDS data
      delete[] rdsdata;
      rdsdata = nullptr;
    }
    else
    {
      stream.SkipBits(8 * (count - 1));
    }
  }
  else
  {
    stream.SkipBits(8 * count);
  }
  return 0;
}
