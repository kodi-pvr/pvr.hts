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
        m_next(0),
        m_channel(0),
        m_content(0),
        m_start(0),
        m_stop(0),
        m_stars(0),
        m_age(0),
        m_aired(0),
        m_season(0),
        m_episode(0),
        m_part(0),
        m_recordingId(0)
      {
      }

      bool operator==(const Event &other) const
      {
        return m_id == other.m_id &&
               m_next == other.m_next &&
               m_channel == other.m_channel &&
               m_content == other.m_content &&
               m_start == other.m_start &&
               m_stop == other.m_stop &&
               m_stars == other.m_stars &&
               m_age == other.m_age &&
               m_aired == other.m_aired &&
               m_season == other.m_season &&
               m_episode == other.m_episode &&
               m_part == other.m_part &&
               m_title == other.m_title &&
               m_subtitle == other.m_subtitle &&
               m_desc == other.m_desc &&
               m_summary == other.m_summary &&
               m_image == other.m_image &&
               m_recordingId == other.m_recordingId;
      }

      bool operator!=(const Event &other) const
      {
        return !(*this == other);
      }

      uint32_t GetNext() const { return m_next; }
      void SetNext(uint32_t next) { m_next = next; }

      uint32_t GetChannel() const { return m_channel; }
      void SetChannel(uint32_t channel) { m_channel = channel; }

      uint32_t GetContent() const { return m_content; }
      void SetContent(uint32_t content) { m_content = content; }

      time_t GetStart() const { return m_start; }
      void SetStart(time_t start) { m_start = start; }

      time_t GetStop() const { return m_stop; }
      void SetStop(time_t stop) { m_stop = stop; }

      uint32_t GetStars() const { return m_stars; }
      void SetStars(uint32_t stars) { m_stars = stars; }

      uint32_t GetAge() const { return m_age; }
      void SetAge(uint32_t age) { m_age = age; }

      time_t GetAired() const { return m_aired; }
      void SetAired(time_t aired) { m_aired = aired; }

      uint32_t GetSeason() const { return m_season; }
      void SetSeason(uint32_t season) { m_season = season; }

      uint32_t GetEpisode() const { return m_episode; }
      void SetEpisode(uint32_t episode) { m_episode = episode; }

      uint32_t GetPart() const { return m_part; }
      void SetPart(uint32_t part) { m_part = part; }

      const std::string& GetTitle() const { return m_title; }
      void SetTitle(const std::string &title) { m_title = title; }

      const std::string& GetSubtitle() const { return m_subtitle; }
      void SetSubtitle(const std::string &subtitle) { m_subtitle = subtitle; }

      // TODO: Rename to GetDescription to match Recording
      const std::string& GetDesc() const { return m_desc; }
      void SetDesc(const std::string &desc) { m_desc = desc; }

      const std::string& GetSummary() const { return m_summary; }
      void SetSummary(const std::string &summary) { m_summary = summary; }

      const std::string& GetImage() const { return m_image; }
      void SetImage(const std::string &image) { m_image = image; }

      uint32_t GetRecordingId() const { return m_recordingId; }
      void SetRecordingId(uint32_t recordingId) { m_recordingId = recordingId; }

    private:
      uint32_t    m_next;
      uint32_t    m_channel;
      uint32_t    m_content;
      time_t      m_start;
      time_t      m_stop;
      uint32_t    m_stars; /* 1 - 5 */
      uint32_t    m_age;   /* years */
      time_t      m_aired;
      uint32_t    m_season;
      uint32_t    m_episode;
      uint32_t    m_part;
      std::string m_title;
      std::string m_subtitle; /* episode name */
      std::string m_desc;
      std::string m_summary;
      std::string m_image;
      uint32_t    m_recordingId;
    };
  }
}
