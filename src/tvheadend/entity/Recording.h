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

#include <algorithm>
#include <map>
#include <string>
#include "xbmc_pvr_types.h"
#include "Entity.h"

// Timer types
#define TIMER_ONCE_MANUAL             (PVR_TIMER_TYPE_NONE + 1)
#define TIMER_ONCE_EPG                (PVR_TIMER_TYPE_NONE + 2)
#define TIMER_ONCE_CREATED_BY_TIMEREC (PVR_TIMER_TYPE_NONE + 3)
#define TIMER_ONCE_CREATED_BY_AUTOREC (PVR_TIMER_TYPE_NONE + 4)
#define TIMER_REPEATING_MANUAL        (PVR_TIMER_TYPE_NONE + 5)
#define TIMER_REPEATING_EPG           (PVR_TIMER_TYPE_NONE + 6)

namespace tvheadend
{
  namespace entity
  {

    class Recording;
    typedef std::pair<uint32_t, Recording> RecordingMapEntry;
    typedef std::map<uint32_t, Recording> Recordings;

    /**
     * Represents a recording or a timer
     * TODO: Create separate classes for recordings and timers since a 
     * recording obviously can't have a "timer type"
     */
    class Recording : public Entity
    {
    public:
      Recording() :
        m_enabled(0),
        m_channel(0),
        m_channelType(0),
        m_eventId(0),
        m_start(0),
        m_stop(0),
        m_startExtra(0),
        m_stopExtra(0),
        m_state(PVR_TIMER_STATE_ERROR),
        m_lifetime(0),
        m_priority(50),   // Kodi default - "normal"
        m_playCount(0),
        m_playPosition(0)
      {
      }

      bool operator==(const Recording &other) const
      {
        return m_id == other.m_id &&
               m_enabled == other.m_enabled &&
               m_channel == other.m_channel &&
               m_channelType == other.m_channelType &&
               m_channelName == other.m_channelName &&
               m_eventId == other.m_eventId &&
               m_start == other.m_start &&
               m_stop == other.m_stop &&
               m_startExtra == other.m_startExtra &&
               m_stopExtra == other.m_stopExtra &&
               m_title == other.m_title &&
               m_path == other.m_path &&
               m_description == other.m_description &&
               m_timerecId == other.m_timerecId &&
               m_autorecId == other.m_autorecId &&
               m_state == other.m_state &&
               m_error == other.m_error &&
               m_lifetime == other.m_lifetime &&
               m_priority == other.m_priority &&
               m_playCount == other.m_playCount &&
               m_playPosition == other.m_playPosition;
      }

      bool operator!=(const Recording &other) const
      {
        return !(*this == other);
      }

      bool IsRecording() const
      {
        return m_state == PVR_TIMER_STATE_COMPLETED ||
               m_state == PVR_TIMER_STATE_ABORTED ||
               m_state == PVR_TIMER_STATE_RECORDING ||
               m_state == PVR_TIMER_STATE_CONFLICT_NOK;
      }

      bool IsTimer() const
      {
        return m_state == PVR_TIMER_STATE_SCHEDULED ||
               m_state == PVR_TIMER_STATE_RECORDING ||
               m_state == PVR_TIMER_STATE_CONFLICT_NOK;
      }

      /**
       * @return the type of timer
       */
      unsigned int GetTimerType() const
      {
        if (!m_timerecId.empty())
          return TIMER_ONCE_CREATED_BY_TIMEREC;
        else if (!m_autorecId.empty())
          return TIMER_ONCE_CREATED_BY_AUTOREC;
        else if (m_eventId != 0)
          return TIMER_ONCE_EPG;
        else
          return TIMER_ONCE_MANUAL;
      }

      bool IsEnabled() const { return m_enabled > 0; }
      void SetEnabled(uint32_t enabled) { m_enabled = enabled; }

      uint32_t GetChannel() const { return m_channel; }
      void SetChannel(uint32_t channel) { m_channel = channel; }

      uint32_t GetChannelType() const { return m_channelType; }
      void SetChannelType(uint32_t channelType) { m_channelType = channelType; }

      const std::string& GetChannelName() const { return m_channelName; }
      void SetChannelName(const std::string &channelName) { m_channelName = channelName; }

      uint32_t GetEventId() const { return m_eventId; }
      void SetEventId(uint32_t eventId) { m_eventId = eventId; }

      // TODO: Change to time_t
      int64_t GetStart() const { return m_start; }
      void SetStart(int64_t start) { m_start = start; }

      // TODO: Change to time_t
      int64_t GetStop() const { return m_stop; }
      void SetStop(int64_t stop) { m_stop = stop; }

      // TODO: Change to time_t
      int64_t GetStartExtra() const { return m_startExtra; }
      void SetStartExtra(int64_t startExtra) { m_startExtra = startExtra; }

      // TODO: Change to time_t
      int64_t GetStopExtra() const { return m_stopExtra; }
      void SetStopExtra(int64_t stopExtra) { m_stopExtra = stopExtra; }

      const std::string& GetTitle() const { return m_title; }
      void SetTitle(const std::string &title) { m_title = title; }

      const std::string& GetSubtitle() const { return m_subtitle; }
      void SetSubtitle(const std::string &subtitle) { m_subtitle = subtitle; }

      const std::string& GetPath() const { return m_path; }
      void SetPath(const std::string &path) { m_path = path; }

      const std::string& GetDescription() const { return m_description; }
      void SetDescription(const std::string &description) { m_description = description; }

      const std::string& GetTimerecId() const { return m_timerecId; }
      void SetTimerecId(const std::string &autorecId) { m_timerecId = autorecId; }

      const std::string& GetAutorecId() const { return m_autorecId; }
      void SetAutorecId(const std::string &title) { m_autorecId = title; }

      PVR_TIMER_STATE GetState() const { return m_state; }
      void SetState(const PVR_TIMER_STATE &state) { m_state = state; }

      const std::string& GetError() const { return m_error; }
      void SetError(const std::string &error) { m_error = error; }

      // Lifetime = the smallest value
      uint32_t GetLifetime() const { return m_lifetime; }
      void SetLifetime(uint32_t lifetime) { m_lifetime = lifetime; }

      uint32_t GetPriority() const { return m_priority; }
      void SetPriority(uint32_t priority) { m_priority = priority; }

      uint32_t GetPlayCount() const { return m_playCount; }
      void SetPlayCount(uint32_t playCount) { m_playCount = playCount; }

      uint32_t GetPlayPosition() const { return m_playPosition; }
      void SetPlayPosition(uint32_t playPosition) { m_playPosition = playPosition; }

    private:
      uint32_t         m_enabled;
      uint32_t         m_channel;
      uint32_t         m_channelType;
      std::string      m_channelName;
      uint32_t         m_eventId;
      int64_t          m_start;
      int64_t          m_stop;
      int64_t          m_startExtra;
      int64_t          m_stopExtra;
      std::string      m_title;
      std::string      m_subtitle;
      std::string      m_path;
      std::string      m_description;
      std::string      m_timerecId;
      std::string      m_autorecId;
      PVR_TIMER_STATE  m_state;
      std::string      m_error;
      uint32_t         m_lifetime;
      uint32_t         m_priority;
      uint32_t         m_playCount;
      uint32_t         m_playPosition;
    };
  }
}
