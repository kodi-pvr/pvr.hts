#pragma once

/*
 *      Copyright (C) 2005-2014 Team XBMC
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

#include <vector>
#include <cstdint>

namespace tvheadend
{
  namespace htsp
  {

    enum EventType
    {
      CHN_UPDATE,
      TAG_UPDATE,
      EPG_UPDATE,
      REC_UPDATE,
    };

    class Event;
    typedef std::vector<Event> EventList;

    /**
     * Represents an event that is a result of a HTSP message. An event requires
     * Kodi to be triggered for an update (the type of update depending on the
     * event type).
     */
    class Event
    {

    public:
      Event(EventType type, uint32_t idx = 0)
          : m_type(type), m_idx(idx)
      {
      }

      bool operator==(const Event &right) const
      {
        return m_type == right.m_type && m_idx == right.m_idx;
      }

      bool operator!=(const Event &right) const
      {
        return !(*this == right);
      }

      EventType GetType() const
      {
        return m_type;
      }

      uint32_t GetIdx() const
      {
        return m_idx;
      }

    private:
      EventType m_type;
      uint32_t m_idx;

    };

  }
}
