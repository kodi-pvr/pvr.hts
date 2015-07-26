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

#include <map>
#include <string>
#include "kodi/xbmc_pvr_types.h"
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
        channel(0),
        eventId(0),
        start(0),
        stop(0),
        startExtra(0),
        stopExtra(0),
        state(PVR_TIMER_STATE_ERROR),
        retention(99), // Kodi default - "99 days"
        priority(50) // Kodi default - "normal"
      {
      }

      bool operator==(const Recording &other) const
      {
        return m_id == other.m_id &&
               channel == other.channel &&
               eventId == other.eventId &&
               start == other.start &&
               stop == other.stop &&
               startExtra == other.startExtra &&
               stopExtra == other.stopExtra &&
               title == other.title &&
               path == other.path &&
               description == other.description &&
               timerecId == other.timerecId &&
               autorecId == other.autorecId &&
               state == other.state &&
               error == other.error &&
               retention == other.retention &&
               priority == other.priority;
      }

      bool operator!=(const Recording &other) const
      {
        return !(this == &other);
      }

      bool IsRecording() const
      {
        return state == PVR_TIMER_STATE_COMPLETED ||
          state == PVR_TIMER_STATE_ABORTED ||
          state == PVR_TIMER_STATE_RECORDING;
      }

      bool IsTimer() const
      {
        return state == PVR_TIMER_STATE_SCHEDULED ||
          state == PVR_TIMER_STATE_RECORDING;
      }

      /**
       * @return the type of timer
       */
      unsigned int GetTimerType() const
      {
        if (!timerecId.empty())
          return TIMER_ONCE_CREATED_BY_TIMEREC;
        else if (!autorecId.empty())
          return TIMER_ONCE_CREATED_BY_AUTOREC;
        else if (eventId != 0)
          return TIMER_ONCE_EPG;
        else
          return TIMER_ONCE_MANUAL;
      }

      uint32_t GetChannel() const { return channel; }
      void SetChannel(uint32_t channel) { this->channel = channel; }

      uint32_t GetEventId() const { return eventId; }
      void SetEventId(uint32_t eventId) { this->eventId = eventId; }

      // TODO: Change to time_t
      int64_t GetStart() const { return start; }
      void SetStart(int64_t start) { this->start = start; }

      // TODO: Change to time_t
      int64_t GetStop() const { return stop; }
      void SetStop(int64_t stop) { this->stop = stop; }

      // TODO: Change to time_t
      int64_t GetStartExtra() const { return startExtra; }
      void SetStartExtra(int64_t startExtra) { this->startExtra = startExtra; }

      // TODO: Change to time_t
      int64_t GetStopExtra() const { return stopExtra; }
      void SetStopExtra(int64_t stopExtra) { this->stopExtra = stopExtra; }

      const std::string& GetTitle() const { return title; }
      void SetTitle(const std::string &title) { this->title = title; }

      const std::string& GetPath() const { return path; }
      void SetPath(const std::string &path) { this->path = path; }

      const std::string& GetDescription() const { return description; }
      void SetDescription(const std::string &description) { this->description = description; }

      const std::string& GetTimerecId() const { return autorecId; }
      void SetTimerecId(const std::string &autorecId) { this->autorecId = autorecId; }

      const std::string& GetAutorecId() const { return title; }
      void SetAutorecId(const std::string &title) { this->title = title; }

      PVR_TIMER_STATE GetState() const { return state; }
      void SetState(const PVR_TIMER_STATE &state) { this->state = state; }

      const std::string& GetError() const { return error; }
      void SetError(const std::string &error) { this->error = error; }

      uint32_t GetRetention() const { return retention; }
      void SetRetention(uint32_t retention) { this->retention = retention; }

      uint32_t GetPriority() const { return priority; }
      void SetPriority(uint32_t priority) { this->priority = priority; }

    private:
      uint32_t         channel;
      uint32_t         eventId;
      int64_t          start;
      int64_t          stop;
      int64_t          startExtra;
      int64_t          stopExtra;
      std::string      title;
      std::string      path;
      std::string      description;
      std::string      timerecId;
      std::string      autorecId;
      PVR_TIMER_STATE  state;
      std::string      error;
      uint32_t         retention;
      uint32_t         priority;
    };
  }
}
