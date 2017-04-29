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

namespace tvheadend {
  namespace status {
    /**
     * Represents the current demuxer packet queue
     */
    struct QueueStatus
    {
      /**
       * Number of data packets in queue
       */
      uint32_t packets;

      /**
       * Number of bytes in queue.
       */
      uint32_t bytes;

      /**
       * Estimated delay of queue (in µs)
       */
      uint32_t delay;

      /**
       * Number of B-frames dropped
       */
      uint32_t bdrops;

      /**
       * Number of P-frames dropped
       */
      uint32_t pdrops;

      /**
       * Number of I-frames dropped
       */
      uint32_t idrops;

      /**
       * Constructor
       */
      QueueStatus() :
          packets(0),
          bytes(0),
          delay(0),
          bdrops(0),
          pdrops(0),
          idrops(0)
      {
      }
    };
  }
}
