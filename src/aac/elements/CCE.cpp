/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "CCE.h"

#include "../BitStream.h"
#include "../huffman/Decoder.h"
#include "ICS.h"
#include "ICSInfo.h"

using namespace aac;
using namespace aac::elements;

void CCE::Decode(BitStream& stream, int profile, int sampleFrequencyIndex)
{
  // 4 bits elem id
  stream.SkipBits(4);

  int couplingPoint = 2 * stream.ReadBit(); // ind sw cce flag
  const int coupledCount = stream.ReadBits(3); // num coupled elements

  int gainCount = 0;
  for (int i = 0; i <= coupledCount; ++i)
  {
    gainCount++;
    const bool channelPair = stream.ReadBool();

    // 4 bits cc target is cpe
    stream.SkipBits(4);

    if (channelPair)
    {
      const int chSelect = stream.ReadBits(2);
      if (chSelect == 3)
        gainCount++;
    }
  }

  couplingPoint += stream.ReadBit(); // cc domain
  couplingPoint |= (couplingPoint >> 1);

  // 1 bit gain element sign, 2 bits gain element scale
  stream.SkipBits(3);

  ICS ics;
  ics.Decode(false, stream, profile, sampleFrequencyIndex);

  const int windowGroupCount = ics.GetInfo().GetWindowGroupCount();
  const int maxSFB = ics.GetInfo().GetMaxSFB();
  const int* sfbCB = ics.GetSfbCB();

  for (int i = 0; i < gainCount; ++i)
  {
    int cge = 1;

    if (i > 0)
    {
      cge = couplingPoint == 2 ? 1 : stream.ReadBit();
      if (cge != 0)
        huffman::Decoder::DecodeScaleFactor(stream);
    }

    if (couplingPoint != 2)
    {
      for (int g = 0; g < windowGroupCount; ++g)
      {
        for (int sfb = 0; sfb < maxSFB; ++sfb)
        {
          if (sfbCB[sfb] != huffman::ZERO_HCB)
          {
            if (cge == 0)
              huffman::Decoder::DecodeScaleFactor(stream);
          }
        }
      }
    }
  }
}
