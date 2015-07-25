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

#include "Entity.h"
#include <map>
#include <cstdint>
#include <string>

namespace tvheadend
{
  namespace entity
  {
    class Event;
    typedef std::pair<uint32_t, Event> EventMapEntry;
    typedef std::map<uint32_t, Event> Events;

    /**
     * Represents an event/programme
     */
    class Event : public Entity
    {
    public:
      Event() :
        next(0),
        channel(0),
        content(0),
        start(0),
        stop(0),
        stars(0),
        age(0),
        aired(0),
        season(0),
        episode(0),
        part(0),
        recordingId(0)
      {
      }

      uint32_t    next;
      uint32_t    channel;
      uint32_t    content;
      time_t      start;
      time_t      stop;
      uint32_t    stars; /* 1 - 5 */
      uint32_t    age;   /* years */
      time_t      aired;
      uint32_t    season;
      uint32_t    episode;
      uint32_t    part;
      std::string title;
      std::string subtitle; /* episode name */
      std::string desc;
      std::string summary;
      std::string image;
      uint32_t    recordingId;
    };
  }
}