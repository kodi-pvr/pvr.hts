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

      bool operator==(const Channel &other) const
      {
        return m_id == other.m_id &&
               num == other.num &&
               numMinor == other.numMinor &&
               radio == other.radio &&
               caid == other.caid &&
               name == other.name && 
               icon == other.icon;
      }

      bool operator!=(const Channel &other) const
      {
        return !(this == &other);
      }

      uint32_t GetNum() const { return num; }
      void SetNum(uint32_t num) { this->num = num; }

      uint32_t GetNumMinor() const { return numMinor; }
      void SetNumMinor(uint32_t numMinor) { this->numMinor = numMinor; }

      bool IsRadio() const { return radio; }
      void SetRadio(bool radio) { this->radio = radio; }

      uint32_t GetCaid() const { return caid; }
      void SetCaid(uint32_t caid) { this->caid = caid; }

      const std::string& GetName() const { return name; }
      void SetName(const std::string &name) { this->name = name; }

      const std::string& GetIcon() const { return icon; }
      void SetIcon(const std::string &icon) { this->icon = icon; }

    private:
      uint32_t         num;
      uint32_t         numMinor;
      bool             radio;
      uint32_t         caid;
      std::string      name;
      std::string      icon;
    };
  }
}