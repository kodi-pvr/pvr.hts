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

#include "TimeRecordings.h"

#include "Tvheadend.h"
#include "tvheadend/utilities/Utilities.h"
#include "tvheadend/utilities/Logger.h"

using namespace P8PLATFORM;
using namespace tvheadend;
using namespace tvheadend::entity;
using namespace tvheadend::utilities;

TimeRecordings::TimeRecordings(CHTSPConnection &conn) :
  m_conn(conn)
{
}

TimeRecordings::~TimeRecordings()
{
}

void TimeRecordings::Connected()
{
  /* Flag all async fields in case they've been deleted */
  for (auto it = m_timeRecordings.begin(); it != m_timeRecordings.end(); ++it)
    it->second.SetDirty(true);
}

void TimeRecordings::SyncDvrCompleted()
{
  utilities::erase_if(m_timeRecordings, [](const TimeRecordingMapEntry &entry)
  {
    return entry.second.IsDirty();
  });
}

int TimeRecordings::GetTimerecTimerCount() const
{
  return m_timeRecordings.size();
}

void TimeRecordings::GetTimerecTimers(std::vector<PVR_TIMER> &timers)
{
  for (auto tit = m_timeRecordings.begin(); tit != m_timeRecordings.end(); ++tit)
  {
    /* Setup entry */
    PVR_TIMER tmr;
    memset(&tmr, 0, sizeof(tmr));

    tmr.iClientIndex       = tit->second.GetId();
    tmr.iClientChannelUid  = (tit->second.GetChannel() > 0) ? tit->second.GetChannel() : PVR_TIMER_ANY_CHANNEL;
    tmr.startTime          = tit->second.GetStart();
    tmr.endTime            = tit->second.GetStop();
    strncpy(tmr.strTitle,
            tit->second.GetName().c_str(), sizeof(tmr.strTitle) - 1);
    strncpy(tmr.strEpgSearchString,
          "", sizeof(tmr.strEpgSearchString) - 1); // n/a for manual timers
    strncpy(tmr.strDirectory,
            tit->second.GetDirectory().c_str(), sizeof(tmr.strDirectory) - 1);
    strncpy(tmr.strSummary, "",
            sizeof(tmr.strSummary) - 1);           // n/a for repeating timers
    tmr.state              = tit->second.IsEnabled()
                              ? PVR_TIMER_STATE_SCHEDULED
                              : PVR_TIMER_STATE_DISABLED;
    tmr.iTimerType         = TIMER_REPEATING_MANUAL;
    tmr.iPriority          = tit->second.GetPriority();
    tmr.iLifetime          = tit->second.GetLifetime();
    tmr.iMaxRecordings     = 0;                    // not supported by tvh
    tmr.iRecordingGroup    = 0;                    // not supported by tvh
    tmr.iPreventDuplicateEpisodes = 0;             // n/a for manual timers
    tmr.firstDay           = 0;                    // not supported by tvh
    tmr.iWeekdays          = tit->second.GetDaysOfWeek();
    tmr.iEpgUid            = PVR_TIMER_NO_EPG_UID; // n/a for manual timers
    tmr.iMarginStart       = 0;                    // n/a for manual timers
    tmr.iMarginEnd         = 0;                    // n/a for manual timers
    tmr.iGenreType         = 0;                    // not supported by tvh?
    tmr.iGenreSubType      = 0;                    // not supported by tvh?
    tmr.bFullTextEpgSearch = false;                // n/a for manual timers
    tmr.iParentClientIndex = 0;

    timers.push_back(tmr);
  }
}

const unsigned int TimeRecordings::GetTimerIntIdFromStringId(const std::string &strId) const
{
  for (auto tit = m_timeRecordings.begin(); tit != m_timeRecordings.end(); ++tit)
  {
    if (tit->second.GetStringId() == strId)
      return tit->second.GetId();
  }
  Logger::Log(LogLevel::LEVEL_ERROR, "Timerec: Unable to obtain int id for string id %s", strId.c_str());
  return 0;
}

const std::string TimeRecordings::GetTimerStringIdFromIntId(unsigned int intId) const
{
  for (auto tit = m_timeRecordings.begin(); tit != m_timeRecordings.end(); ++tit)
  {
    if (tit->second.GetId() == intId)
      return  tit->second.GetStringId();
  }

  Logger::Log(LogLevel::LEVEL_ERROR, "Timerec: Unable to obtain string id for int id %s", intId);
  return "";
}

PVR_ERROR TimeRecordings::SendTimerecAdd(const PVR_TIMER &timer)
{
  return SendTimerecAddOrUpdate(timer, false);
}

PVR_ERROR TimeRecordings::SendTimerecUpdate(const PVR_TIMER &timer)
{
  if (m_conn.GetProtocol() >= 25)
    return SendTimerecAddOrUpdate(timer, true);

  /* Note: there is no "updateTimerec" htsp method for htsp version < 25, thus delete + add. */
  PVR_ERROR error = SendTimerecDelete(timer);

  if (error == PVR_ERROR_NO_ERROR)
    error = SendTimerecAdd(timer);

  return error;
}

PVR_ERROR TimeRecordings::SendTimerecAddOrUpdate(const PVR_TIMER &timer, bool update)
{
  uint32_t u32;
  const std::string method = update ? "updateTimerecEntry" : "addTimerecEntry";

  /* Build message */
  htsmsg_t *m = htsmsg_create_map();

  if (update)
  {
    std::string strId = GetTimerStringIdFromIntId(timer.iClientIndex);
    if (strId.empty())
    {
      htsmsg_destroy(m);
      return PVR_ERROR_FAILED;
    }

    htsmsg_add_str(m, "id",       strId.c_str());            // Autorec DVR Entry ID (string!)
  }

  char title[PVR_ADDON_NAME_STRING_LENGTH+6];
  const char *titleExt = "%F-%R"; // timerec title should contain the pattern (e.g. %F-%R) for the generated recording files. Scary...
  snprintf(title, sizeof(title), "%s-%s", timer.strTitle, titleExt);

  htsmsg_add_str(m, "name",       timer.strTitle);
  htsmsg_add_str(m, "title",      title);
  struct tm *tm_start = localtime(&timer.startTime);
  htsmsg_add_u32(m, "start",      tm_start->tm_hour * 60 + tm_start->tm_min); // start time in minutes from midnight
  struct tm *tm_stop = localtime(&timer.endTime);
  htsmsg_add_u32(m, "stop",       tm_stop->tm_hour  * 60 + tm_stop->tm_min);  // end time in minutes from midnight

  if (m_conn.GetProtocol() >= 25)
  {
    htsmsg_add_u32(m, "removal",   timer.iLifetime);          // remove from disk
    htsmsg_add_u32(m, "retention", DVR_RET_ONREMOVE);         // remove from tvh database
    htsmsg_add_s64(m, "channelId", timer.iClientChannelUid);  // channelId is signed for >= htspv25
  }
  else
  {
    htsmsg_add_u32(m, "retention", timer.iLifetime);          // remove from tvh database
    htsmsg_add_u32(m, "channelId", timer.iClientChannelUid);  // channelId is unsigned for < htspv25
  }

  htsmsg_add_u32(m, "daysOfWeek", timer.iWeekdays);
  htsmsg_add_u32(m, "priority",   timer.iPriority);
  htsmsg_add_u32(m, "enabled",    timer.state == PVR_TIMER_STATE_DISABLED ? 0 : 1);

  /* Note: As a result of internal filename cleanup, for "directory" == "/", */
  /*       tvh would put recordings into a folder named "-". Not a big issue */
  /*       but ugly.                                                         */
  if (strcmp(timer.strDirectory, "/") != 0)
    htsmsg_add_str(m, "directory", timer.strDirectory);

  /* Send and Wait */
  {
    CLockObject lock(m_conn.Mutex());
    m = m_conn.SendAndWait(method.c_str(), m);
  }

  if (m == NULL)
    return PVR_ERROR_SERVER_ERROR;

  /* Check for error */
  if (htsmsg_get_u32(m, "success", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed %s response: 'success' missing", method.c_str());
    u32 = PVR_ERROR_FAILED;
  }
  htsmsg_destroy(m);

  return u32 == 1 ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
}

PVR_ERROR TimeRecordings::SendTimerecDelete(const PVR_TIMER &timer)
{
  uint32_t u32;

  std::string strId = GetTimerStringIdFromIntId(timer.iClientIndex);
  if (strId.empty())
    return PVR_ERROR_FAILED;

  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "id", strId.c_str()); // Timerec DVR Entry ID (string!)

  /* Send and Wait */
  {
    CLockObject lock(m_conn.Mutex());
    m = m_conn.SendAndWait("deleteTimerecEntry", m);
  }

  if (m == NULL)
    return PVR_ERROR_SERVER_ERROR;

  /* Check for error */
  if (htsmsg_get_u32(m, "success", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed deleteTimerecEntry response: 'success' missing");
  }
  htsmsg_destroy(m);

  return u32 == 1 ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
}

bool TimeRecordings::ParseTimerecAddOrUpdate(htsmsg_t *msg, bool bAdd)
{
  const char *str;
  uint32_t u32;
  int32_t s32;

  /* Validate/set mandatory fields */
  if ((str = htsmsg_get_str(msg, "id")) == NULL)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed timerecEntryAdd/timerecEntryUpdate: 'id' missing");
    return false;
  }

  /* Locate/create entry */
  TimeRecording &rec = m_timeRecordings[std::string(str)];
  rec.SetStringId(std::string(str));
  rec.SetDirty(false);

  /* Validate/set fields mandatory for timerecEntryAdd */

  if (!htsmsg_get_u32(msg, "enabled", &u32))
  {
    rec.SetEnabled(u32);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed timerecEntryAdd: 'enabled' missing");
    return false;
  }

  if (!htsmsg_get_u32(msg, "daysOfWeek", &u32))
  {
    rec.SetDaysOfWeek(u32);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed timerecEntryAdd: 'daysOfWeek' missing");
    return false;
  }

  if (m_conn.GetProtocol() >= 25)
  {
    if (!htsmsg_get_u32(msg, "removal", &u32))
    {
      rec.SetLifetime(u32);
    }
    else if (bAdd)
    {
      Logger::Log(LogLevel::LEVEL_ERROR, "malformed timerecEntryAdd: 'removal' missing");
      return false;
    }
  }
  else
  {
    if (!htsmsg_get_u32(msg, "retention", &u32))
    {
      rec.SetLifetime(u32);
    }
    else if (bAdd)
    {
      Logger::Log(LogLevel::LEVEL_ERROR, "malformed timerecEntryAdd: 'retention' missing");
      return false;
    }
  }

  if (!htsmsg_get_u32(msg, "priority", &u32))
  {
    rec.SetPriority(u32);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed timerecEntryAdd: 'priority' missing");
    return false;
  }

  if (!htsmsg_get_s32(msg, "start", &s32))
  {
    rec.SetStart(s32);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed timerecEntryAdd: 'start' missing");
    return false;
  }

  if (!htsmsg_get_s32(msg, "stop", &s32))
  {
    rec.SetStop(s32);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed timerecEntryAdd: 'stop' missing");
    return false;
  }

  /* Add optional fields */
  if ((str = htsmsg_get_str(msg, "title")) != NULL)
  {
    rec.SetTitle(str);
  }

  if ((str = htsmsg_get_str(msg, "name")) != NULL)
  {
    rec.SetName(str);
  }

  if ((str = htsmsg_get_str(msg, "directory")) != NULL)
  {
    rec.SetDirectory(str);
  }

  if ((str = htsmsg_get_str(msg, "owner")) != NULL)
  {
    rec.SetOwner(str);
  }

  if ((str = htsmsg_get_str(msg, "creator")) != NULL)
  {
    rec.SetCreator(str);
  }

  if (!htsmsg_get_u32(msg, "channel", &u32))
  {
    rec.SetChannel(u32);
  }
  else
  {
    /* A timerec can also have an empty channel field,
     * the user can delete a channel or even an automated bouquet update can do this
     * let kodi know that no channel is assigned, in this way the user can assign a new channel to this timerec
     * note: "any channel" will be interpreted as "no channel" for timerecs by kodi */
    rec.SetChannel(PVR_TIMER_ANY_CHANNEL);
  }

  return true;
}

bool TimeRecordings::ParseTimerecDelete(htsmsg_t *msg)
{
  const char *id;

  /* Validate/set mandatory fields */
  if ((id = htsmsg_get_str(msg, "id")) == NULL)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed timerecEntryDelete: 'id' missing");
    return false;
  }
  Logger::Log(LogLevel::LEVEL_TRACE, "delete timerec entry %s", id);

  /* Erase */
  m_timeRecordings.erase(std::string(id));

  return true;
}
