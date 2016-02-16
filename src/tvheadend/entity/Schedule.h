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
#include <vector>
#include "Entity.h"

namespace tvheadend
{
  namespace entity
  {
    class Schedule;
    typedef std::pair<int, Schedule> ScheduleMapEntry;
    typedef std::map<int, Schedule> Schedules;

    typedef std::pair<uint32_t, Entity> EventUidsMapEntry;
    typedef std::map<uint32_t, Entity> EventUids;

    /**
     * Represents a schedule. A schedule has a channel and a bunch of events. 
     * The schedule ID matches the channel it belongs to.
     */
    class Schedule : public Entity
    {
    public:
      virtual void SetDirty(bool dirty);

      /**
       * @return read-write reference to the events in this schedule
       */
      EventUids& GetEvents();

      /**
       * @return read-only reference to the events in this schedule
       */
      const EventUids& GetEvents() const;

    private:
      EventUids m_events; // event uids
    };
  }
}
