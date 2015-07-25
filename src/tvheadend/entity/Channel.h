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

#include <cstdint>
#include <string>
#include <map>
#include "Entity.h"

namespace tvheadend
{
  namespace entity
  {

    class Channel;
    typedef std::pair<uint32_t, Channel> ChannelMapEntry;
    typedef std::map<uint32_t, Channel> Channels;

    /**
     * Represents a channel
     */
    class Channel : public Entity
    {
    public:
      Channel() :
        num(0),
        numMinor(0),
        radio(false),
        caid(0)
      {
      }

      bool operator<(const Channel &right) const
      {
        return num < right.num;
      }

      uint32_t         num;
      uint32_t         numMinor;
      bool             radio;
      uint32_t         caid;
      std::string      name;
      std::string      icon;
    };
  }
}