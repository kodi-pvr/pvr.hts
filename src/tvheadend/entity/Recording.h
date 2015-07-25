#pragma once

/*
 *      Copyright (C) 2005-2011 Team XBMC
 *      http://www.xbmc.org
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

#include <map>
#include <string>
#include "kodi/xbmc_pvr_types.h"
#include "Entity.h"

namespace tvheadend
{
  namespace entity
  {

    class Recording;
    typedef std::pair<uint32_t, Recording> RecordingMapEntry;
    typedef std::map<uint32_t, Recording> Recordings;

    /**
     * Represents a recording or a timer
     */
    class Recording : public Entity
    {
    public:
      Recording() :
        channel(0),
        eventId(0),
        start(0),
        stop(0),
        startExtra(0),
        stopExtra(0),
        state(PVR_TIMER_STATE_ERROR),
        retention(99), // Kodi default - "99 days"
        priority(50) // Kodi default - "normal"
      {
      }

      bool IsRecording() const
      {
        return state == PVR_TIMER_STATE_COMPLETED ||
          state == PVR_TIMER_STATE_ABORTED ||
          state == PVR_TIMER_STATE_RECORDING;
      }

      bool IsTimer() const
      {
        return state == PVR_TIMER_STATE_SCHEDULED ||
          state == PVR_TIMER_STATE_RECORDING;
      }

      uint32_t         channel;
      uint32_t         eventId;
      int64_t          start;
      int64_t          stop;
      int64_t          startExtra;
      int64_t          stopExtra;
      std::string      title;
      std::string      path;
      std::string      description;
      std::string      timerecId;
      std::string      autorecId;
      PVR_TIMER_STATE  state;
      std::string      error;
      uint32_t         retention;
      uint32_t         priority;
    };
  }
}
