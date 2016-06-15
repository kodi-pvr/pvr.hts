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
#include "../../HTSPTypes.h"

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
        m_num(0),
        m_numMinor(0),
        m_type(CHANNEL_TYPE_OTHER),
        m_caid(0)
      {
      }

      bool operator<(const Channel &right) const
      {
        return m_num < right.m_num;
      }

      bool operator==(const Channel &other) const
      {
        return m_id == other.m_id &&
               m_num == other.m_num &&
               m_numMinor == other.m_numMinor &&
               m_type == other.m_type &&
               m_caid == other.m_caid &&
               m_name == other.m_name &&
               m_icon == other.m_icon;
      }

      bool operator!=(const Channel &other) const
      {
        return !(*this == other);
      }

      uint32_t GetNum() const { return m_num; }
      void SetNum(uint32_t num) { m_num = num; }

      uint32_t GetNumMinor() const { return m_numMinor; }
      void SetNumMinor(uint32_t numMinor) { m_numMinor = numMinor; }

      uint32_t GetType() const { return m_type; }
      void SetType(uint32_t type) { m_type = type; }

      uint32_t GetCaid() const { return m_caid; }
      void SetCaid(uint32_t caid) { m_caid = caid; }

      const std::string& GetName() const { return m_name; }
      void SetName(const std::string &name) { m_name = name; }

      const std::string& GetIcon() const { return m_icon; }
      void SetIcon(const std::string &icon) { m_icon = icon; }

    private:
      uint32_t         m_num;
      uint32_t         m_numMinor;
      uint32_t         m_type;
      uint32_t         m_caid;
      std::string      m_name;
      std::string      m_icon;
    };
  }
}
