/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "FIL.h"

#include "../BitStream.h"

using namespace aac;
using namespace aac::elements;

void FIL::Decode(BitStream& stream)
{
  int count = stream.ReadBits(4);
  if (count == 15)
    count += stream.ReadBits(8) - 1;

  if (count > 0)
    stream.SkipBits(count * 8);
}
