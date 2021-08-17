/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "CPE.h"

#include "../BitStream.h"
#include "../SampleFrequency.h"
#include "ICS.h"
#include "ICSInfo.h"

#include <stdexcept>

using namespace aac;
using namespace aac::elements;

void CPE::Decode(BitStream& stream, int profile, int sampleFrequencyIndex)
{
  if (sampleFrequencyIndex == SAMPLE_FREQUENCY_NONE)
    throw std::invalid_argument("aac::elements::CPE::Decode - Invalid sample frequency");

  // 4 bits elem id
  stream.SkipBits(4);

  ICS icsL;
  ICS icsR;

  const bool commonWindow = stream.ReadBool();
  if (commonWindow)
  {
    ICSInfo& icsInfoL = icsL.GetInfo();
    icsInfoL.Decode(false, stream, profile, sampleFrequencyIndex);
    icsR.GetInfo().SetData(icsInfoL);

    // decode mid-side-stereo info

    static constexpr int TYPE_ALL_0 = 0;
    static constexpr int TYPE_USED = 1;
    static constexpr int TYPE_ALL_1 = 2;
    static constexpr int TYPE_RESERVED = 3;

    const int msMaskPresent = stream.ReadBits(2);
    if (msMaskPresent == TYPE_USED)
    {
      //      for (int i = 0; i < icsInfoL.GetWindowGroupCount(); ++i)
      //      {
      //        for (int sfb = 0; sfb < icsInfoL.GetMaxSFB(); ++sfb)
      //        {
      //          // 1 bit ms used
      //          m_stream.SkipBit();
      //        }
      //      }
      stream.SkipBits(icsInfoL.GetWindowGroupCount() * icsInfoL.GetMaxSFB());
    }
    else if (msMaskPresent != TYPE_ALL_0 && msMaskPresent != TYPE_ALL_1 &&
             msMaskPresent != TYPE_RESERVED)
    {
      throw std::out_of_range("aac::elements::CPE::Decode - Invalid ms mask present value");
    }
  }

  // decode left ics
  icsL.Decode(commonWindow, stream, profile, sampleFrequencyIndex);

  // decode right ics
  icsR.Decode(commonWindow, stream, profile, sampleFrequencyIndex);
}
