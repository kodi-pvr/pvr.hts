/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "Entity.h"

#include <map>
#include <vector>

namespace tvheadend
{
namespace entity
{

class Schedule;
typedef std::pair<int, Schedule> ScheduleMapEntry;
typedef std::map<int, Schedule> Schedules;

typedef std::pair<uint32_t, Entity> EventUidsMapEntry;
typedef std::map<uint32_t, Entity> EventUids;

/**
 * Represents a schedule. A schedule has a channel and a bunch of events.
 * The schedule ID matches the channel it belongs to.
 */
class Schedule : public Entity
{
public:
  virtual void SetDirty(bool dirty);

  /**
   * @return read-write reference to the events in this schedule
   */
  EventUids& GetEvents();

  /**
   * @return read-only reference to the events in this schedule
   */
  const EventUids& GetEvents() const;

private:
  EventUids m_events; // event uids
};

} // namespace entity
} // namespace tvheadend
