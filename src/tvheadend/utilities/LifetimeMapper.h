/*
 *  Copyright (C) 2017-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "../HTSPTypes.h"

namespace tvheadend
{
namespace utilities
{

/**
 * Maps "lifetime" values from Kodi to Tvheadend and vica versa
 */
class LifetimeMapper
{
public:
  static int TvhToKodi(uint32_t tvhLifetime)
  {
    // pvr addon api: addon defined special values must be less than zero
    if (tvhLifetime == DVR_RET_DVRCONFIG)
      return -3;
    else if (tvhLifetime == DVR_RET_SPACE)
      return -2;
    else if (tvhLifetime == DVR_RET_FOREVER)
      return -1;
    else
      return tvhLifetime; // lifetime in days
  }

  static uint32_t KodiToTvh(int kodiLifetime)
  {
    if (kodiLifetime == -3)
      return DVR_RET_DVRCONFIG;
    else if (kodiLifetime == -2)
      return DVR_RET_SPACE;
    else if (kodiLifetime == -1)
      return DVR_RET_FOREVER;
    else
      return kodiLifetime; // lifetime in days
  }
};

} // namespace utilities
} // namespace tvheadend
