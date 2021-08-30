/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "TimeRecordings.h"

#include "HTSPConnection.h"
#include "entity/Recording.h"
#include "utilities/LifetimeMapper.h"
#include "utilities/Logger.h"
#include "utilities/Utilities.h"

#include <cstring>
#include <ctime>

using namespace tvheadend;
using namespace tvheadend::entity;
using namespace tvheadend::utilities;

TimeRecordings::TimeRecordings(HTSPConnection& conn) : m_conn(conn)
{
}

TimeRecordings::~TimeRecordings()
{
}

void TimeRecordings::RebuildState()
{
  /* Flag all async fields in case they've been deleted */
  for (auto& rec : m_timeRecordings)
    rec.second.SetDirty(true);
}

void TimeRecordings::SyncDvrCompleted()
{
  utilities::erase_if(m_timeRecordings,
                      [](const TimeRecordingMapEntry& entry) { return entry.second.IsDirty(); });
}

int TimeRecordings::GetTimerecTimerCount() const
{
  return m_timeRecordings.size();
}

void TimeRecordings::GetTimerecTimers(std::vector<kodi::addon::PVRTimer>& timers)
{
  for (const auto& rec : m_timeRecordings)
  {
    /* Setup entry */
    kodi::addon::PVRTimer tmr;

    tmr.SetClientIndex(rec.second.GetId());
    tmr.SetClientChannelUid((rec.second.GetChannel() > 0) ? rec.second.GetChannel()
                                                          : PVR_TIMER_ANY_CHANNEL);
    tmr.SetStartTime(rec.second.GetStart());
    tmr.SetEndTime(rec.second.GetStop());
    tmr.SetTitle(rec.second.GetName());
    tmr.SetEPGSearchString(""); // n/a for manual timers
    tmr.SetDirectory(rec.second.GetDirectory());
    tmr.SetSummary(""); // n/a for repeating timers
    tmr.SetState(rec.second.IsEnabled() ? PVR_TIMER_STATE_SCHEDULED : PVR_TIMER_STATE_DISABLED);
    tmr.SetTimerType(TIMER_REPEATING_MANUAL);
    tmr.SetPriority(rec.second.GetPriority());
    tmr.SetLifetime(rec.second.GetLifetime());
    tmr.SetMaxRecordings(0); // not supported by tvh
    tmr.SetRecordingGroup(0); // not supported by tvh
    tmr.SetPreventDuplicateEpisodes(0); // n/a for manual timers
    tmr.SetFirstDay(0); // not supported by tvh
    tmr.SetWeekdays(rec.second.GetDaysOfWeek());
    tmr.SetEPGUid(PVR_TIMER_NO_EPG_UID); // n/a for manual timers
    tmr.SetMarginStart(0); // n/a for manual timers
    tmr.SetMarginEnd(0); // n/a for manual timers
    tmr.SetGenreType(0); // not supported by tvh?
    tmr.SetGenreSubType(0); // not supported by tvh?
    tmr.SetFullTextEpgSearch(false); // n/a for manual timers
    tmr.SetParentClientIndex(0);

    timers.emplace_back(tmr);
  }
}

const unsigned int TimeRecordings::GetTimerIntIdFromStringId(const std::string& strId) const
{
  for (const auto& rec : m_timeRecordings)
  {
    if (rec.second.GetStringId() == strId)
      return rec.second.GetId();
  }
  Logger::Log(LogLevel::LEVEL_ERROR, "Timerec: Unable to obtain int id for string id %s",
              strId.c_str());
  return 0;
}

const std::string TimeRecordings::GetTimerStringIdFromIntId(unsigned int intId) const
{
  for (const auto& rec : m_timeRecordings)
  {
    if (rec.second.GetId() == intId)
      return rec.second.GetStringId();
  }

  Logger::Log(LogLevel::LEVEL_ERROR, "Timerec: Unable to obtain string id for int id %s", intId);
  return "";
}

PVR_ERROR TimeRecordings::SendTimerecAdd(const kodi::addon::PVRTimer& timer)
{
  return SendTimerecAddOrUpdate(timer, false);
}

PVR_ERROR TimeRecordings::SendTimerecUpdate(const kodi::addon::PVRTimer& timer)
{
  return SendTimerecAddOrUpdate(timer, true);
}

PVR_ERROR TimeRecordings::SendTimerecAddOrUpdate(const kodi::addon::PVRTimer& timer, bool update)
{
  const std::string method = update ? "updateTimerecEntry" : "addTimerecEntry";

  /* Build message */
  htsmsg_t* m = htsmsg_create_map();

  if (update)
  {
    std::string strId = GetTimerStringIdFromIntId(timer.GetClientIndex());
    if (strId.empty())
    {
      htsmsg_destroy(m);
      return PVR_ERROR_FAILED;
    }

    htsmsg_add_str(m, "id", strId.c_str()); // Autorec DVR Entry ID (string!)
  }

  const char* titleExt =
      "%F-%R"; // timerec title should contain the pattern (e.g. %F-%R) for the generated recording files. Scary...
  std::string title = timer.GetTitle() + "-" + titleExt;

  htsmsg_add_str(m, "name", timer.GetTitle().c_str());
  htsmsg_add_str(m, "title", title.c_str());
  time_t startTime = timer.GetStartTime();
  struct tm* tm_start = std::localtime(&startTime);
  htsmsg_add_u32(m, "start",
                 tm_start->tm_hour * 60 + tm_start->tm_min); // start time in minutes from midnight
  time_t endTime = timer.GetEndTime();
  struct tm* tm_stop = std::localtime(&endTime);
  htsmsg_add_u32(m, "stop",
                 tm_stop->tm_hour * 60 + tm_stop->tm_min); // end time in minutes from midnight
  htsmsg_add_u32(m, "removal", timer.GetLifetime()); // remove from disk
  htsmsg_add_s64(m, "channelId", timer.GetClientChannelUid());
  htsmsg_add_u32(m, "daysOfWeek", timer.GetWeekdays());
  htsmsg_add_u32(m, "priority", timer.GetPriority());
  htsmsg_add_u32(m, "enabled", timer.GetState() == PVR_TIMER_STATE_DISABLED ? 0 : 1);

  /* Note: As a result of internal filename cleanup, for "directory" == "/", */
  /*       tvh would put recordings into a folder named "-". Not a big issue */
  /*       but ugly.                                                         */
  if (timer.GetDirectory() != "/")
    htsmsg_add_str(m, "directory", timer.GetDirectory().c_str());

  /* Send and Wait */
  {
    std::unique_lock<std::recursive_mutex> lock(m_conn.Mutex());
    m = m_conn.SendAndWait(lock, method.c_str(), m);
  }

  if (!m)
    return PVR_ERROR_SERVER_ERROR;

  /* Check for error */
  uint32_t u32 = 0;
  if (htsmsg_get_u32(m, "success", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed %s response: 'success' missing", method.c_str());
    u32 = PVR_ERROR_FAILED;
  }
  htsmsg_destroy(m);

  return u32 == 1 ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
}

PVR_ERROR TimeRecordings::SendTimerecDelete(const kodi::addon::PVRTimer& timer)
{
  std::string strId = GetTimerStringIdFromIntId(timer.GetClientIndex());
  if (strId.empty())
    return PVR_ERROR_FAILED;

  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_str(m, "id", strId.c_str()); // Timerec DVR Entry ID (string!)

  /* Send and Wait */
  {
    std::unique_lock<std::recursive_mutex> lock(m_conn.Mutex());
    m = m_conn.SendAndWait(lock, "deleteTimerecEntry", m);
  }

  if (!m)
    return PVR_ERROR_SERVER_ERROR;

  /* Check for error */
  uint32_t u32 = 0;
  if (htsmsg_get_u32(m, "success", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed deleteTimerecEntry response: 'success' missing");
  }
  htsmsg_destroy(m);

  return u32 == 1 ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
}

bool TimeRecordings::ParseTimerecAddOrUpdate(htsmsg_t* msg, bool bAdd)
{
  /* Validate/set mandatory fields */
  const char* str = htsmsg_get_str(msg, "id");
  if (!str)
  {
    Logger::Log(LogLevel::LEVEL_ERROR,
                "malformed timerecEntryAdd/timerecEntryUpdate: 'id' missing");
    return false;
  }

  /* Locate/create entry */
  TimeRecording& rec = m_timeRecordings[std::string(str)];
  rec.SetStringId(std::string(str));
  rec.SetDirty(false);

  /* Validate/set fields mandatory for timerecEntryAdd */

  uint32_t u32 = 0;
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

  if (!htsmsg_get_u32(msg, "removal", &u32))
  {
    rec.SetLifetime(u32);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed timerecEntryAdd: 'removal' missing");
    return false;
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

  int32_t s32 = 0;
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
  str = htsmsg_get_str(msg, "title");
  if (str)
    rec.SetTitle(str);

  str = htsmsg_get_str(msg, "name");
  if (str)
    rec.SetName(str);

  str = htsmsg_get_str(msg, "directory");
  if (str)
    rec.SetDirectory(str);

  str = htsmsg_get_str(msg, "owner");
  if (str)
    rec.SetOwner(str);

  str = htsmsg_get_str(msg, "creator");
  if (str)
    rec.SetCreator(str);

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

bool TimeRecordings::ParseTimerecDelete(htsmsg_t* msg)
{
  /* Validate/set mandatory fields */
  const char* id = htsmsg_get_str(msg, "id");
  if (!id)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed timerecEntryDelete: 'id' missing");
    return false;
  }
  Logger::Log(LogLevel::LEVEL_TRACE, "delete timerec entry %s", id);

  /* Erase */
  m_timeRecordings.erase(std::string(id));

  return true;
}
