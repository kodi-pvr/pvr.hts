/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <vector>

extern "C"
{
#include "libhts/htsmsg.h"

#include <sys/types.h>
}

#include "entity/TimeRecording.h"

#include "kodi/addon-instance/pvr/Timers.h"

namespace tvheadend
{

class HTSPConnection;

class TimeRecordings
{
public:
  TimeRecordings(HTSPConnection& conn);
  ~TimeRecordings();

  /* state updates */
  void RebuildState();
  void SyncDvrCompleted();

  /* data access */
  int GetTimerecTimerCount() const;
  void GetTimerecTimers(std::vector<kodi::addon::PVRTimer>& timers);
  const unsigned int GetTimerIntIdFromStringId(const std::string& strId) const;

  /* client to server messages */
  PVR_ERROR SendTimerecAdd(const kodi::addon::PVRTimer& timer);
  PVR_ERROR SendTimerecUpdate(const kodi::addon::PVRTimer& timer);
  PVR_ERROR SendTimerecDelete(const kodi::addon::PVRTimer& timer);

  /* server to client messages */
  bool ParseTimerecAddOrUpdate(htsmsg_t* msg, bool bAdd);
  bool ParseTimerecDelete(htsmsg_t* msg);

private:
  const std::string GetTimerStringIdFromIntId(unsigned int intId) const;
  PVR_ERROR SendTimerecAddOrUpdate(const kodi::addon::PVRTimer& timer, bool update);

  HTSPConnection& m_conn;
  tvheadend::entity::TimeRecordingsMap m_timeRecordings;
};

} // namespace tvheadend
