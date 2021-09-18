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

namespace huffman
{

static constexpr int ZERO_HCB = 0;

static constexpr int NOISE_HCB = 13;
static constexpr int INTENSITY_HCB2 = 14;
static constexpr int INTENSITY_HCB = 15;

class Decoder
{
public:
  Decoder() = delete;

  static int DecodeScaleFactor(aac::BitStream& bitstream);
  static void DecodeSpectralData(aac::BitStream& bitstream, int cb, int data[], int off);
};

} // namespace huffman
} // namespace aac
