/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Decoder.h"

#include "../BitStream.h"
#include "Codebooks.h"

#include <cmath>
#include <stdexcept>
#include <string>

using namespace aac;
using namespace aac::huffman;

namespace
{

constexpr bool UNSIGNED[] = {false, false, true, true, false, false, true, true, true, true, true};
constexpr int QUAD_LEN = 4;
constexpr int PAIR_LEN = 2;

int FindOffset(BitStream& stream, const cb_table_entry table[])
{
  int off = 0;
  int len = table[off].length;
  int cw = stream.ReadBits(len);

  while (cw != table[off].codeword)
  {
    off++;
    const int j = table[off].length - len;
    len = table[off].length;
    cw <<= j;
    cw |= stream.ReadBits(j);
  }

  return off;
}

void SignValues(BitStream& stream, int data[], int off, int len)
{
  for (int i = off; i < off + len; ++i)
  {
    if (data[i] != 0)
    {
      if (stream.ReadBool())
        data[i] = -data[i];
    }
  }
}

int GetEscape(BitStream& stream, int s)
{
  const bool neg = (s < 0);

  int i = 4;
  while (stream.ReadBool())
  {
    i++;
  }

  const int j = stream.ReadBits(i) | (1 << i);
  return (neg ? -j : j);
}

} // namespace

int Decoder::DecodeScaleFactor(BitStream& bitstream)
{
  const int offset = FindOffset(bitstream, HCB_SF);
  return HCB_SF[offset].bit;
}

void Decoder::DecodeSpectralData(BitStream& bitstream, int cb, int data[], int off)
{
  const cb_table_entry* HCB = CODEBOOKS[cb - 1];
  const int offset = FindOffset(bitstream, HCB);

  data[off] = HCB[offset].bit;
  data[off + 1] = HCB[offset].value1;
  if (cb < 5)
  {
    data[off + 2] = HCB[offset].value2;
    data[off + 3] = HCB[offset].value3;
  }

  if (cb < 11)
  {
    if (UNSIGNED[cb - 1])
      SignValues(bitstream, data, off, cb < 5 ? QUAD_LEN : PAIR_LEN);
  }
  else if (cb == 11 || cb > 15)
  {
    SignValues(bitstream, data, off, cb < 5 ? QUAD_LEN : PAIR_LEN);
    if (std::abs(data[off]) == 16)
      data[off] = GetEscape(bitstream, data[off]);
    if (std::abs(data[off + 1]) == 16)
      data[off + 1] = GetEscape(bitstream, data[off + 1]);
  }
  else
  {
    throw std::logic_error(
        "aac::huffman::Decoder::DecodeSpectralData - Unknown spectral codebook: " +
        std::to_string(cb));
  }
}
