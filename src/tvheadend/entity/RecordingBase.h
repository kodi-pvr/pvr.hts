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
#include <string>
#include <cstdint>
#include <ctime>

namespace tvheadend
{
  namespace entity
  {
    class RecordingBase : public Entity
    {
    protected:
      RecordingBase(const std::string &id = "");
      bool operator==(const RecordingBase &right);
      bool operator!=(const RecordingBase &right);

    public:
      std::string GetStringId() const;
      void SetStringId(const std::string &id);

      bool IsEnabled() const;
      void SetEnabled(uint32_t enabled);

      int GetDaysOfWeek() const;
      void SetDaysOfWeek(uint32_t daysOfWeek);

      uint32_t GetLifetime() const;
      void SetLifetime(uint32_t retention);

      uint32_t GetPriority() const;
      void SetPriority(uint32_t priority);

      const std::string &GetTitle() const;
      void SetTitle(const std::string &title);

      const std::string &GetName() const;
      void SetName(const std::string &name);

      const std::string &GetDirectory() const;
      void SetDirectory(const std::string &directory);

      void SetOwner(const std::string &owner);
      void SetCreator(const std::string &creator);

      uint32_t GetChannel() const;
      void SetChannel(uint32_t channel);

    protected:
      static time_t LocaltimeToUTC(int32_t lctime);

    private:
      static unsigned int GetNextIntId();

      std::string m_sid;        // ID (string!) of dvr[Time|Auto]recEntry.
      uint32_t m_enabled;       // If [time|auto]rec entry is enabled (activated).
      uint32_t m_daysOfWeek;    // Bitmask - Days of week (0x01 = Monday, 0x40 = Sunday, 0x7f = Whole Week, 0 = Not set).
      uint32_t m_lifetime;      // Lifetime (in days).
      uint32_t m_priority;      // Priority (0 = Important, 1 = High, 2 = Normal, 3 = Low, 4 = Unimportant).
      std::string m_title;      // Title (pattern) for the recording files.
      std::string m_name;       // Name.
      std::string m_directory;  // Directory for the recording files.
      std::string m_owner;      // Owner.
      std::string m_creator;    // Creator.
      uint32_t m_channel;       // Channel ID.
    };
  }
}
