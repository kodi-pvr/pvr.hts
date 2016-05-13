#pragma once

/*
 *      Copyright (C) 2005-2015 Team Kodi
 *      http://kodi.tv
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
 *  along with Kodi; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <vector>

extern "C"
{
#include <sys/types.h>
#include "libhts/htsmsg.h"
}

#include "libXBMC_pvr.h"
#include "tvheadend/entity/TimeRecording.h"

class CHTSPConnection;

class TimeRecordings
{
public:
  TimeRecordings(CHTSPConnection &conn);
  ~TimeRecordings();

  /* state updates */
  void Connected();
  void SyncDvrCompleted();

  /* data access */
  int  GetTimerecTimerCount() const;
  void GetTimerecTimers(std::vector<PVR_TIMER> &timers);
  const unsigned int GetTimerIntIdFromStringId(const std::string &strId) const;

  /* client to server messages */
  PVR_ERROR SendTimerecAdd   (const PVR_TIMER &timer);
  PVR_ERROR SendTimerecUpdate(const PVR_TIMER &timer);
  PVR_ERROR SendTimerecDelete(const PVR_TIMER &timer);

  /* server to client messages */
  bool ParseTimerecAddOrUpdate(htsmsg_t *msg, bool bAdd);
  bool ParseTimerecDelete(htsmsg_t *msg);

private:
  const std::string GetTimerStringIdFromIntId(unsigned int intId) const;
  PVR_ERROR SendTimerecAddOrUpdate(const PVR_TIMER &timer, bool update);

  CHTSPConnection                      &m_conn;
  tvheadend::entity::TimeRecordingsMap  m_timeRecordings;
};
