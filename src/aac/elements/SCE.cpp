/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "SCE.h"

#include "../BitStream.h"
#include "../elements/ICS.h"

using namespace aac;
using namespace aac::elements;

void SCE::Decode(BitStream& stream, int profile, int sampleFrequencyIndex)
{
  // 4 bits elem id
  stream.SkipBits(4);

  ICS ics;
  ics.Decode(false, stream, profile, sampleFrequencyIndex);
}
