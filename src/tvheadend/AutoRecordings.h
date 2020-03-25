/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
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

#include "entity/AutoRecording.h"
#include "kodi/libXBMC_pvr.h"

namespace tvheadend
{

class HTSPConnection;

class AutoRecordings
{
public:
  AutoRecordings(HTSPConnection& conn);
  ~AutoRecordings();

  /* state updates */
  void Connected();
  void SyncDvrCompleted();

  /* data access */
  int GetAutorecTimerCount() const;
  void GetAutorecTimers(std::vector<PVR_TIMER>& timers);
  const unsigned int GetTimerIntIdFromStringId(const std::string& strId) const;

  /* client to server messages */
  PVR_ERROR SendAutorecAdd(const PVR_TIMER& timer);
  PVR_ERROR SendAutorecUpdate(const PVR_TIMER& timer);
  PVR_ERROR SendAutorecDelete(const PVR_TIMER& timer);

  /* server to client messages */
  bool ParseAutorecAddOrUpdate(htsmsg_t* msg, bool bAdd);
  bool ParseAutorecDelete(htsmsg_t* msg);

private:
  const std::string GetTimerStringIdFromIntId(unsigned int intId) const;
  PVR_ERROR SendAutorecAddOrUpdate(const PVR_TIMER& timer, bool update);

  HTSPConnection& m_conn;
  tvheadend::entity::AutoRecordingsMap m_autoRecordings;
};

} // namespace tvheadend
