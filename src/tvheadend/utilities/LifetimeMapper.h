#pragma once

/*
 *      Copyright (C) 2017 Team Kodi
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "../../HTSPTypes.h"

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
        if (tvhLifetime == DVR_RET_SPACE)
          return -2;
        else if (tvhLifetime == DVR_RET_FOREVER)
          return -1;
        else
          return tvhLifetime; // lifetime in days
      }

      static uint32_t KodiToTvh(int kodiLifetime)
      {
        if (kodiLifetime == -2)
          return DVR_RET_SPACE;
        else if (kodiLifetime == -1)
          return DVR_RET_FOREVER;
        else
          return kodiLifetime; // lifetime in days
      }
    };
  }
}
