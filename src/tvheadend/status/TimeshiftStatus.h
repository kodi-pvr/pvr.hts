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

namespace tvheadend
{
  namespace status
  {
    /**
     * Represents the current timeshift status
     */
    struct TimeshiftStatus
    {

      /**
       * Whether the buffer is full or not
       */
      bool full;

      /**
       * Current position relative to live
       */
      int64_t shift;

      /**
       * PTS of the first frame in the buffer
       */
      int64_t start;

      /**
       * PTS of the last frame in the buffer
       */
      int64_t end;

      /**
       * Constructor
       */
      TimeshiftStatus() :
          full(0),
          shift(0),
          start(0),
          end(0)
      {
      }
    };
  }
}
