/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "PCE.h"

#include "../BitStream.h"

using namespace aac;
using namespace aac::elements;

void PCE::Decode(BitStream& stream)
{
  // 4 bits elem id
  stream.SkipBits(4);

  m_profile = stream.ReadBits(2);
  m_sampleFrequencyIndex = stream.ReadBits(4);

  const int frontChannelElementsCount = stream.ReadBits(4);
  const int sideChannelElementsCount = stream.ReadBits(4);
  const int backChannelElementsCount = stream.ReadBits(4);
  const int lfeChannelElementsCount = stream.ReadBits(2);
  const int assocDataElementsCount = stream.ReadBits(3);
  const int validCCElementsCount = stream.ReadBits(4);

  const bool monoMixdownPresent = stream.ReadBool();
  if (monoMixdownPresent)
  {
    // 4 bits mono mixdown element number
    stream.SkipBits(4);
  }

  const bool stereoMixdownPresent = stream.ReadBool();
  if (stereoMixdownPresent)
  {
    // 4 bits stereo mixdown element number
    stream.SkipBits(4);
  }

  const bool matrixMixdownIdxPresent = stream.ReadBool();
  if (matrixMixdownIdxPresent)
  {
    // 2 bits matrix mixdown idx, 1 bit pseudo surround enable
    stream.SkipBits(3);
  }

  //  for (int i = 0; i < frontChannelElementsCount; ++i)
  //  {
  //    // 1 bit front element is cpe, 4 bits front element tagselect
  //    stream.SkipBits(5);
  //  }

  //  for (int i = 0; i < sideChannelElementsCount; ++i)
  //  {
  //    // 1 bit side element is cpe, 4 bits side element tagselect
  //    stream.SkipBits(5);
  //  }

  //  for (int i = 0; i < backChannelElementsCount; ++i)
  //  {
  //    // 1 bit back element is cpe, 4 bits back element tagselect
  //    stream.SkipBits(5);
  //  }

  //  for (int i = 0; i < lfeChannelElementsCount; ++i)
  //  {
  //    // 4 bits lfe element tag select
  //    stream.SkipBits(4);
  //  }

  //  for (int i = 0; i < assocDataElementsCount; ++i)
  //  {
  //    // 4 bits assoc data element tag select
  //    stream.SkipBits(4);
  //  }

  //  for (int i = 0; i < validCCElementsCount; ++i)
  //  {
  //    // 1 bit cc element is ind sw, 4 bits valid cc element tag select
  //    stream.SkipBits(5);
  //  }

  stream.SkipBits(5 * frontChannelElementsCount + 5 * sideChannelElementsCount +
                  5 * backChannelElementsCount + 4 * lfeChannelElementsCount +
                  4 * assocDataElementsCount + 5 * validCCElementsCount);

  stream.ByteAlign();

  const int commentFieldBytes = stream.ReadBits(8);
  //  for(int i = 0; i < commentFieldBytes; ++i)
  //  {
  //    // 8 bits comment field data
  //    stream.SkipBits(8);
  //  }
  stream.SkipBits(8 * commentFieldBytes);
}
