/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Schedule.h"

using namespace tvheadend::entity;

void Schedule::SetDirty(bool dirty)
{
  Entity::SetDirty(dirty);

  if (dirty)
  {
    // Mark all events as dirty too
    for (auto& entry : m_events)
      entry.second.SetDirty(dirty);
  }
}

EventUids& Schedule::GetEvents()
{
  return m_events;
}

const EventUids& Schedule::GetEvents() const
{
  return m_events;
}
