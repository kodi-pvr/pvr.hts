/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "AutoRecordings.h"

#include "CustomTimerProperties.h"
#include "HTSPConnection.h"
#include "InstanceSettings.h"
#include "entity/Recording.h"
#include "utilities/LifetimeMapper.h"
#include "utilities/Logger.h"
#include "utilities/Utilities.h"

#include <cstring>
#include <ctime>
#include <regex>

using namespace tvheadend;
using namespace tvheadend::entity;
using namespace tvheadend::utilities;

AutoRecordings::AutoRecordings(const std::shared_ptr<InstanceSettings>& settings,
                               HTSPConnection& conn,
                               Profiles& dvrConfigs)
  : m_settings(settings),
    m_conn(conn),
    m_customTimerProps({CUSTOM_PROP_ID_AUTOREC_START, CUSTOM_PROP_ID_AUTOREC_STARTWINDOW,
                        CUSTOM_PROP_ID_AUTOREC_BROADCASTTYPE, CUSTOM_PROP_ID_DVR_CONFIGURATION,
                        CUSTOM_PROP_ID_DVR_COMMENT},
                       conn,
                       dvrConfigs)
{
}

AutoRecordings::~AutoRecordings()
{
}

void AutoRecordings::RebuildState()
{
  /* Flag all async fields in case they've been deleted */
  for (auto& rec : m_autoRecordings)
    rec.second.SetDirty(true);
}

void AutoRecordings::SyncDvrCompleted()
{
  utilities::erase_if(m_autoRecordings,
                      [](const AutoRecordingMapEntry& entry) { return entry.second.IsDirty(); });
}

int AutoRecordings::GetAutorecTimerCount() const
{
  return static_cast<int>(m_autoRecordings.size());
}

void AutoRecordings::GetAutorecTimers(std::vector<kodi::addon::PVRTimer>& timers)
{
  timers.reserve(timers.size() + m_autoRecordings.size());

  for (const auto& rec : m_autoRecordings)
  {
    /* Setup entry */
    kodi::addon::PVRTimer tmr;

    tmr.SetClientIndex(rec.second.GetId());
    tmr.SetClientChannelUid((rec.second.GetChannel() > 0) ? rec.second.GetChannel()
                                                          : PVR_TIMER_ANY_CHANNEL);
    if (rec.second.GetName().empty()) // timers created on backend may not contain a name
      tmr.SetTitle(rec.second.GetTitle());
    else
      tmr.SetTitle(rec.second.GetName());
    tmr.SetEPGSearchString(rec.second.GetTitle());
    tmr.SetDirectory(rec.second.GetDirectory());
    tmr.SetSummary(""); // n/a for repeating timers
    tmr.SetSeriesLink(rec.second.GetSeriesLink());
    tmr.SetState(rec.second.IsEnabled() ? PVR_TIMER_STATE_SCHEDULED : PVR_TIMER_STATE_DISABLED);
    tmr.SetTimerType(rec.second.GetSeriesLink().empty() ? TIMER_REPEATING_EPG
                                                        : TIMER_REPEATING_SERIESLINK);
    tmr.SetPriority(rec.second.GetPriority());
    tmr.SetLifetime(rec.second.GetLifetime());
    tmr.SetMaxRecordings(0); // not supported by tvh
    tmr.SetRecordingGroup(0); // not supported by tvh
    tmr.SetPreventDuplicateEpisodes(rec.second.GetDupDetect());
    tmr.SetFirstDay(0); // not supported by tvh
    tmr.SetWeekdays(rec.second.GetDaysOfWeek());
    tmr.SetEPGUid(PVR_TIMER_NO_EPG_UID); // n/a for repeating timers
    tmr.SetMarginStart(static_cast<unsigned int>(rec.second.GetMarginStart()));
    tmr.SetMarginEnd(static_cast<unsigned int>(rec.second.GetMarginEnd()));
    tmr.SetGenreType(0); // not supported by tvh?
    tmr.SetGenreSubType(0); // not supported by tvh?
    tmr.SetFullTextEpgSearch(rec.second.GetFulltext());
    tmr.SetParentClientIndex(0);

    /* Custom props. */
    tmr.SetCustomProperties(m_customTimerProps.GetProperties(rec.second));

    timers.emplace_back(std::move(tmr));
  }
}

const unsigned int AutoRecordings::GetTimerIntIdFromStringId(const std::string& strId) const
{
  for (const auto& rec : m_autoRecordings)
  {
    if (rec.second.GetStringId() == strId)
      return rec.second.GetId();
  }
  Logger::Log(LogLevel::LEVEL_ERROR, "Autorec: Unable to obtain int id for string id %s",
              strId.c_str());
  return 0;
}

const std::string AutoRecordings::GetTimerStringIdFromIntId(unsigned int intId) const
{
  for (const auto& rec : m_autoRecordings)
  {
    if (rec.second.GetId() == intId)
      return rec.second.GetStringId();
  }

  Logger::Log(LogLevel::LEVEL_ERROR, "Autorec: Unable to obtain string id for int id %s", intId);
  return "";
}

const std::vector<kodi::addon::PVRSettingDefinition> AutoRecordings::GetCustomSettingDefinitions()
    const
{
  return m_customTimerProps.GetSettingDefinitions();
}

PVR_ERROR AutoRecordings::SendAutorecAdd(const kodi::addon::PVRTimer& timer)
{
  return SendAutorecAddOrUpdate(timer, false);
}

PVR_ERROR AutoRecordings::SendAutorecUpdate(const kodi::addon::PVRTimer& timer)
{
  return SendAutorecAddOrUpdate(timer, true);
}

PVR_ERROR AutoRecordings::SendAutorecAddOrUpdate(const kodi::addon::PVRTimer& timer, bool update)
{
  const std::string method = update ? "updateAutorecEntry" : "addAutorecEntry";

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

    htsmsg_add_str(m, "id", strId.c_str()); // Autorec DVR Entry ID (string!
  }

  htsmsg_add_str(m, "name", timer.GetTitle().c_str());

  /* epg search data match string */
  std::string searchString = timer.GetEPGSearchString();
  if (!m_settings->GetAutorecUseRegEx())
  {
    // escape regex special chars
    static const std::regex specialChars(R"([-[\]{}()*+?.,\^$|#])");
    searchString = std::regex_replace(searchString, specialChars, R"(\$&)");
  }
  htsmsg_add_str(m, "title", searchString.c_str());

  /* fulltext epg search:                                                                          */
  /* "title" not empty && !fulltext => match strEpgSearchString against episode title only         */
  /* "title" not empty && fulltext  => match strEpgSearchString against episode title, episode     */
  /*                                   subtitle, episode summary and episode description (HTSPv19) */
  htsmsg_add_u32(m, "fulltext", timer.GetFullTextEpgSearch() ? 1 : 0);

  htsmsg_add_s64(m, "startExtra", timer.GetMarginStart());
  htsmsg_add_s64(m, "stopExtra", timer.GetMarginEnd());
  htsmsg_add_u32(m, "removal", timer.GetLifetime()); // remove from disk
  htsmsg_add_s64(m, "channelId", timer.GetClientChannelUid()); // -1 = any
  htsmsg_add_u32(m, "daysOfWeek", timer.GetWeekdays());
  htsmsg_add_u32(m, "dupDetect", timer.GetPreventDuplicateEpisodes());
  htsmsg_add_u32(m, "priority", timer.GetPriority());
  htsmsg_add_u32(m, "enabled", timer.GetState() == PVR_TIMER_STATE_DISABLED ? 0 : 1);

  /* Note: As a result of internal filename cleanup, for "directory" == "/", */
  /*       tvh would put recordings into a folder named "-". Not a big issue */
  /*       but ugly.                                                         */
  if (timer.GetDirectory() != "/")
    htsmsg_add_str(m, "directory", timer.GetDirectory().c_str());

  /* series link */
  if (timer.GetTimerType() == TIMER_REPEATING_SERIESLINK)
    htsmsg_add_str(m, "serieslinkUri", timer.GetSeriesLink().c_str());

  /* Custom props. */
  m_customTimerProps.AppendPropertiesToHTSPMessage(timer.GetCustomProperties(), m);

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

PVR_ERROR AutoRecordings::SendAutorecDelete(const kodi::addon::PVRTimer& timer)
{
  std::string strId = GetTimerStringIdFromIntId(timer.GetClientIndex());
  if (strId.empty())
    return PVR_ERROR_FAILED;

  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_str(m, "id", strId.c_str()); // Autorec DVR Entry ID (string!)

  /* Send and Wait */
  {
    std::unique_lock<std::recursive_mutex> lock(m_conn.Mutex());
    m = m_conn.SendAndWait(lock, "deleteAutorecEntry", m);
  }

  if (!m)
    return PVR_ERROR_SERVER_ERROR;

  /* Check for error */
  uint32_t u32 = 0;
  if (htsmsg_get_u32(m, "success", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed deleteAutorecEntry response: 'success' missing");
  }
  htsmsg_destroy(m);

  return u32 == 1 ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
}

bool AutoRecordings::ParseAutorecAddOrUpdate(htsmsg_t* msg, bool bAdd)
{
  /* Validate/set mandatory fields */
  const char* str = htsmsg_get_str(msg, "id");
  if (!str)
  {
    Logger::Log(LogLevel::LEVEL_ERROR,
                "malformed autorecEntryAdd/autorecEntryUpdate: 'id' missing");
    return false;
  }

  /* Locate/create entry */
  AutoRecording& rec = m_autoRecordings[std::string(str)];
  rec.SetStringId(std::string(str));
  rec.SetDirty(false);

  /* Validate/set fields mandatory for autorecEntryAdd */

  uint32_t u32 = 0;
  if (!htsmsg_get_u32(msg, "enabled", &u32))
  {
    rec.SetEnabled(u32);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryAdd: 'enabled' missing");
    return false;
  }

  if (!htsmsg_get_u32(msg, "removal", &u32))
  {
    rec.SetLifetime(u32);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryAdd: 'removal' missing");
    return false;
  }

  if (!htsmsg_get_u32(msg, "daysOfWeek", &u32))
  {
    rec.SetDaysOfWeek(u32);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryAdd: 'daysOfWeek' missing");
    return false;
  }

  if (!htsmsg_get_u32(msg, "priority", &u32))
  {
    rec.SetPriority(u32);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryAdd: 'priority' missing");
    return false;
  }

  int32_t s32 = 0;
  if (!htsmsg_get_s32(msg, "start", &s32))
  {
    rec.SetStartWindowBegin(s32);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryAdd: 'start' missing");
    return false;
  }

  if (!htsmsg_get_s32(msg, "startWindow", &s32))
  {
    rec.SetStartWindowEnd(s32);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryAdd: 'startWindow' missing");
    return false;
  }

  int64_t s64 = 0;
  if (!htsmsg_get_s64(msg, "startExtra", &s64))
  {
    rec.SetMarginStart(s64);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryAdd: 'startExtra' missing");
    return false;
  }

  if (!htsmsg_get_s64(msg, "stopExtra", &s64))
  {
    rec.SetMarginEnd(s64);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryAdd: 'stopExtra' missing");
    return false;
  }

  if (!htsmsg_get_u32(msg, "dupDetect", &u32))
  {
    rec.SetDupDetect(u32);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryAdd: 'dupDetect' missing");
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
    rec.SetChannel(u32);
  else
    rec.SetChannel(PVR_TIMER_ANY_CHANNEL); // an empty channel field = any channel

  if (!htsmsg_get_u32(msg, "fulltext", &u32))
    rec.SetFulltext(u32);

  str = htsmsg_get_str(msg, "serieslinkUri");
  if (str)
    rec.SetSeriesLink(str);

  if (!htsmsg_get_u32(msg, "broadcastType", &u32))
    rec.SetBroadcastType(u32);

  str = htsmsg_get_str(msg, "configId");
  if (str)
    rec.SetConfigUuid(str);

  str = htsmsg_get_str(msg, "comment");
  if (str)
    rec.SetComment(str);

  return true;
}

bool AutoRecordings::ParseAutorecDelete(htsmsg_t* msg)
{
  /* Validate/set mandatory fields */
  const char* id = htsmsg_get_str(msg, "id");
  if (!id)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryDelete: 'id' missing");
    return false;
  }
  Logger::Log(LogLevel::LEVEL_TRACE, "delete autorec entry %s", id);

  /* Erase */
  m_autoRecordings.erase(std::string(id));

  return true;
}
