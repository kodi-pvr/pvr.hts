#pragma once

/*
 *      Copyright (C) 2005-2015 Team Kodi
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

#include <cstdint>
#include <string>

namespace tvheadend
{
  namespace status
  {
    /**
     * Represents the current signal quality
     */
    struct Quality
    {

      /**
       * Frontend status
       */
      std::string fe_status;

      /**
       * Signal to noise ratio
       */
      uint32_t fe_snr;

      /**
       * Signal status percentage
       */
      uint32_t fe_signal;

      /**
       * Bit error rate
       */
      uint32_t fe_ber;

      /**
       * Uncorrected blocks
       */
      uint32_t fe_unc;

      /**
       * Constructor
       */
      Quality() :
          fe_snr(0),
          fe_signal(0),
          fe_ber(0),
          fe_unc(0)
      {
      }

      /**
       * Clears the current status
       */
      void Clear()
      {
        fe_status.clear();
        fe_snr = 0;
        fe_signal = 0;
        fe_ber = 0;
        fe_unc = 0;
      }
    };
  }
}
