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
#include <vector>
#include "Entity.h"
#include "../../HTSPTypes.h"

namespace tvheadend
{
  namespace entity
  {
    class Tag;
    typedef std::pair<uint32_t, Tag> TagMapEntry;
    typedef std::map<uint32_t, Tag> Tags;

    /**
     * Represents a channel tag
     */
    class Tag : public Entity
    {
    public:
      Tag();

      bool operator==(const Tag &right);
      bool operator!=(const Tag &right);

      uint32_t GetIndex() const;
      void SetIndex(uint32_t index);

      const std::string& GetName() const;
      void SetName(const std::string& name);

      void SetIcon(const std::string& icon);

      const std::vector<uint32_t>& GetChannels() const;
      std::vector<uint32_t>& GetChannels();

      bool ContainsChannelType(channel_type_t eType) const;

    private:
      uint32_t              m_index;
      std::string           m_name;
      std::string           m_icon;
      std::vector<uint32_t> m_channels;
    };
  }
}

