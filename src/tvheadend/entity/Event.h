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

      bool operator==(const Event &other) const
      {
        return m_id == other.m_id &&
               next == other.next &&
               channel == other.channel &&
               content == other.content &&
               start == other.start &&
               stop == other.stop &&
               stars == other.stars &&
               age == other.age &&
               aired == other.aired &&
               season == other.season &&
               episode == other.episode &&
               part == other.part &&
               title == other.title &&
               subtitle == other.subtitle &&
               desc == other.desc &&
               summary == other.summary &&
               image == other.image &&
               recordingId == other.recordingId;
      }

      bool operator!=(const Event &other) const
      {
        return !(this == &other);
      }

      uint32_t GetNext() const { return next; }
      void SetNext(uint32_t next) { this->next = next; }

      uint32_t GetChannel() const { return channel; }
      void SetChannel(uint32_t channel) { this->channel = channel; }

      uint32_t GetContent() const { return content; }
      void SetContent(uint32_t content) { this->content = content; }

      time_t GetStart() const { return start; }
      void SetStart(time_t start) { this->start = start; }

      time_t GetStop() const { return stop; }
      void SetStop(time_t stop) { this->stop = stop; }

      uint32_t GetStars() const { return stars; }
      void SetStars(uint32_t stars) { this->stars = stars; }

      uint32_t GetAge() const { return age; }
      void SetAge(uint32_t age) { this->age = age; }

      time_t GetAired() const { return aired; }
      void SetAired(time_t aired) { this->aired = aired; }

      uint32_t GetSeason() const { return season; }
      void SetSeason(uint32_t season) { this->season = season; }

      uint32_t GetEpisode() const { return episode; }
      void SetEpisode(uint32_t episode) { this->episode = episode; }

      uint32_t GetPart() const { return part; }
      void SetPart(uint32_t part) { this->part = part; }

      const std::string& GetTitle() const { return title; }
      void SetTitle(const std::string &title) { this->title = title; }

      const std::string& GetSubtitle() const { return subtitle; }
      void SetSubtitle(const std::string &subtitle) { this->subtitle = subtitle; }

      // TODO: Rename to GetDescription to match Recording
      const std::string& GetDesc() const { return desc; }
      void SetDesc(const std::string &desc) { this->desc = desc; }

      const std::string& GetSummary() const { return summary; }
      void SetSummary(const std::string &summary) { this->summary = summary; }

      const std::string& GetImage() const { return image; }
      void SetImage(const std::string &image) { this->image = image; }

      uint32_t GetRecordingId() const { return recordingId; }
      void SetRecordingId(uint32_t recordingId) { this->recordingId = recordingId; }

    private:
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
