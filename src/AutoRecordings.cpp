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

#include "AutoRecordings.h"

#include "Tvheadend.h"
#include "tvheadend/Settings.h"
#include "tvheadend/utilities/Utilities.h"
#include "tvheadend/utilities/Logger.h"

using namespace P8PLATFORM;
using namespace tvheadend;
using namespace tvheadend::entity;
using namespace tvheadend::utilities;

AutoRecordings::AutoRecordings(CHTSPConnection &conn) :
  m_conn(conn)
{
}

AutoRecordings::~AutoRecordings()
{
}

void AutoRecordings::Connected()
{
  /* Flag all async fields in case they've been deleted */
  for (auto it = m_autoRecordings.begin(); it != m_autoRecordings.end(); ++it)
    it->second.SetDirty(true);
}

void AutoRecordings::SyncDvrCompleted()
{
  utilities::erase_if(m_autoRecordings, [](const AutoRecordingMapEntry &entry)
  {
    return entry.second.IsDirty();
  });
}

int AutoRecordings::GetAutorecTimerCount() const
{
  return m_autoRecordings.size();
}

void AutoRecordings::GetAutorecTimers(std::vector<PVR_TIMER> &timers)
{
  for (auto tit = m_autoRecordings.begin(); tit != m_autoRecordings.end(); ++tit)
  {
    /* Setup entry */
    PVR_TIMER tmr;
    memset(&tmr, 0, sizeof(tmr));

    tmr.iClientIndex       = tit->second.GetId();
    tmr.iClientChannelUid  = (tit->second.GetChannel() > 0) ? tit->second.GetChannel() : PVR_TIMER_ANY_CHANNEL;
    tmr.startTime          = tit->second.GetStart();
    tmr.endTime            = tit->second.GetStop();
    if (tmr.startTime == 0)
      tmr.bStartAnyTime = true;
    if (tmr.endTime == 0)
      tmr.bEndAnyTime = true;

    if (!tmr.bStartAnyTime && tmr.bEndAnyTime)
      tmr.endTime = tmr.startTime + 60 * 60; // Nominal 1 hour duration
    if (tmr.bStartAnyTime && !tmr.bEndAnyTime)
      tmr.startTime = tmr.endTime - 60 * 60; // Nominal 1 hour duration
    if (tmr.bStartAnyTime && tmr.bEndAnyTime)
    {
      tmr.startTime = time(NULL); // now
      tmr.endTime = tmr.startTime + 60 * 60; // Nominal 1 hour duration
    }

    if (tit->second.GetName().empty()) // timers created on backend may not contain a name
      strncpy(tmr.strTitle,
              tit->second.GetTitle().c_str(), sizeof(tmr.strTitle) - 1);
    else
      strncpy(tmr.strTitle,
              tit->second.GetName().c_str(), sizeof(tmr.strTitle) - 1);
    strncpy(tmr.strEpgSearchString,
            tit->second.GetTitle().c_str(), sizeof(tmr.strEpgSearchString) - 1);
    strncpy(tmr.strDirectory,
            tit->second.GetDirectory().c_str(), sizeof(tmr.strDirectory) - 1);
    strncpy(tmr.strSummary,
            "", sizeof(tmr.strSummary) - 1);       // n/a for repeating timers
    tmr.state              = tit->second.IsEnabled()
                              ? PVR_TIMER_STATE_SCHEDULED
                              : PVR_TIMER_STATE_DISABLED;
    tmr.iTimerType         = TIMER_REPEATING_EPG;
    tmr.iPriority          = tit->second.GetPriority();
    tmr.iLifetime          = tit->second.GetLifetime();
    tmr.iMaxRecordings     = 0;                    // not supported by tvh
    tmr.iRecordingGroup    = 0;                    // not supported by tvh

    if (m_conn.GetProtocol() >= 20)
      tmr.iPreventDuplicateEpisodes = tit->second.GetDupDetect();
    else
      tmr.iPreventDuplicateEpisodes = 0;           // not supported by tvh

    tmr.firstDay           = 0;                    // not supported by tvh
    tmr.iWeekdays          = tit->second.GetDaysOfWeek();
    tmr.iEpgUid            = PVR_TIMER_NO_EPG_UID; // n/a for repeating timers
    tmr.iMarginStart       = static_cast<unsigned int>(tit->second.GetMarginStart());
	tmr.iMarginEnd         = static_cast<unsigned int>(tit->second.GetMarginEnd());
    tmr.iGenreType         = 0;                    // not supported by tvh?
    tmr.iGenreSubType      = 0;                    // not supported by tvh?
    tmr.bFullTextEpgSearch = tit->second.GetFulltext();
    tmr.iParentClientIndex = 0;

    timers.push_back(tmr);
  }
}

const unsigned int AutoRecordings::GetTimerIntIdFromStringId(const std::string &strId) const
{
  for (auto tit = m_autoRecordings.begin(); tit != m_autoRecordings.end(); ++tit)
  {
    if (tit->second.GetStringId() == strId)
      return tit->second.GetId();
  }
  Logger::Log(LogLevel::LEVEL_ERROR, "Autorec: Unable to obtain int id for string id %s", strId.c_str());
  return 0;
}

const std::string AutoRecordings::GetTimerStringIdFromIntId(unsigned int intId) const
{
  for (auto tit = m_autoRecordings.begin(); tit != m_autoRecordings.end(); ++tit)
  {
    if (tit->second.GetId() == intId)
      return  tit->second.GetStringId();
  }

  Logger::Log(LogLevel::LEVEL_ERROR, "Autorec: Unable to obtain string id for int id %s", intId);
  return "";
}

PVR_ERROR AutoRecordings::SendAutorecAdd(const PVR_TIMER &timer)
{
  return SendAutorecAddOrUpdate(timer, false);
}

PVR_ERROR AutoRecordings::SendAutorecUpdate(const PVR_TIMER &timer)
{
  if (m_conn.GetProtocol() >= 25)
    return SendAutorecAddOrUpdate(timer, true);

  /* Note: there is no "updateAutorec" htsp method for htsp version < 25, thus delete + add. */
  PVR_ERROR error = SendAutorecDelete(timer);

  if (error == PVR_ERROR_NO_ERROR)
    error = SendAutorecAdd(timer);

  return error;
}

PVR_ERROR AutoRecordings::SendAutorecAddOrUpdate(const PVR_TIMER &timer, bool update)
{
  uint32_t u32;
  const std::string method = update ? "updateAutorecEntry" : "addAutorecEntry";

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

    htsmsg_add_str(m, "id",       strId.c_str());            // Autorec DVR Entry ID (string!
  }

  htsmsg_add_str(m, "name",       timer.strTitle);

  /* epg search data match string (regexp) */
  htsmsg_add_str(m, "title",      timer.strEpgSearchString);

  /* fulltext epg search:                                                                          */
  /* "title" not empty && !fulltext => match strEpgSearchString against episode title only         */
  /* "title" not empty && fulltext  => match strEpgSearchString against episode title, episode     */
  /*                                   subtitle, episode summary and episode description (HTSPv19) */
  if (m_conn.GetProtocol() >= 20)
    htsmsg_add_u32(m, "fulltext",   timer.bFullTextEpgSearch ? 1 : 0);

  htsmsg_add_s64(m, "startExtra", timer.iMarginStart);
  htsmsg_add_s64(m, "stopExtra",  timer.iMarginEnd);

  if (m_conn.GetProtocol() >= 25)
  {
    htsmsg_add_u32(m, "removal",   timer.iLifetime);            // remove from disk
    htsmsg_add_u32(m, "retention", DVR_RET_ONREMOVE);           // remove from tvh database
    htsmsg_add_s64(m, "channelId", timer.iClientChannelUid);    // channelId is signed for >= htspv25, -1 = any
  }
  else
  {
    htsmsg_add_u32(m, "retention", timer.iLifetime);            // remove from tvh database

    if (timer.iClientChannelUid >= 0)
      htsmsg_add_u32(m, "channelId", timer.iClientChannelUid);  // channelId is unsigned for < htspv25, not sending = any
  }

  htsmsg_add_u32(m, "daysOfWeek",  timer.iWeekdays);

  if (m_conn.GetProtocol() >= 20)
    htsmsg_add_u32(m, "dupDetect", timer.iPreventDuplicateEpisodes);

  htsmsg_add_u32(m, "priority",    timer.iPriority);
  htsmsg_add_u32(m, "enabled",     timer.state == PVR_TIMER_STATE_DISABLED ? 0 : 1);

  /* Note: As a result of internal filename cleanup, for "directory" == "/", */
  /*       tvh would put recordings into a folder named "-". Not a big issue */
  /*       but ugly.                                                         */
  if (strcmp(timer.strDirectory, "/") != 0)
    htsmsg_add_str(m, "directory", timer.strDirectory);


  /* bAutorecApproxTime enabled:  => start time in kodi = approximate start time in tvh     */
  /*                              => 'approximate'      = starting window / 2               */
  /*                                                                                        */
  /* bAutorecApproxTime disabled: => start time in kodi = begin of starting window in tvh   */
  /*                              => end time in kodi   = end of starting window in tvh     */
  const Settings &settings = Settings::GetInstance();

  if (settings.GetAutorecApproxTime())
  {
    /* Not sending causes server to set start and startWindow to any time */
    if (timer.startTime > 0 && !timer.bStartAnyTime)
    {
      struct tm *tm_start = localtime(&timer.startTime);
      int32_t startWindowBegin = tm_start->tm_hour * 60 + tm_start->tm_min - settings.GetAutorecMaxDiff();
      int32_t startWindowEnd = tm_start->tm_hour * 60 + tm_start->tm_min + settings.GetAutorecMaxDiff();

      /* Past midnight correction */
      if (startWindowBegin < 0)
        startWindowBegin += (24 * 60);
      if (startWindowEnd > (24 * 60))
        startWindowEnd -= (24 * 60);

      htsmsg_add_s32(m, "start", startWindowBegin);
      htsmsg_add_s32(m, "startWindow", startWindowEnd);
    }
  }
  else
  {
    if (timer.startTime > 0 && !timer.bStartAnyTime)
    {
      /* Exact start time (minutes from midnight). */
      struct tm *tm_start = localtime(&timer.startTime);
      htsmsg_add_s32(m, "start", tm_start->tm_hour * 60 + tm_start->tm_min);
    }
    else
      htsmsg_add_s32(m, "start", 25 * 60); // -1 or not sending causes server to set start and startWindow to any time

    if (timer.endTime > 0 && !timer.bEndAnyTime)
    {
      /* Exact stop time (minutes from midnight). */
      struct tm *tm_stop = localtime(&timer.endTime);
      htsmsg_add_s32(m, "startWindow", tm_stop->tm_hour * 60 + tm_stop->tm_min);
    }
    else
      htsmsg_add_s32(m, "startWindow", 25 * 60); // -1 or not sending causes server to set start and startWindow to any time
  }

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

PVR_ERROR AutoRecordings::SendAutorecDelete(const PVR_TIMER &timer)
{
  uint32_t u32;

  std::string strId = GetTimerStringIdFromIntId(timer.iClientIndex);
  if (strId.empty())
    return PVR_ERROR_FAILED;

  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "id", strId.c_str()); // Autorec DVR Entry ID (string!)

  /* Send and Wait */
  {
    CLockObject lock(m_conn.Mutex());
    m = m_conn.SendAndWait("deleteAutorecEntry", m);
  }

  if (m == NULL)
    return PVR_ERROR_SERVER_ERROR;

  /* Check for error */
  if (htsmsg_get_u32(m, "success", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed deleteAutorecEntry response: 'success' missing");
  }
  htsmsg_destroy(m);

  return u32 == 1 ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
}

bool AutoRecordings::ParseAutorecAddOrUpdate(htsmsg_t *msg, bool bAdd)
{
  const char *str;
  uint32_t u32;
  int32_t s32;
  int64_t s64;

  /* Validate/set mandatory fields */
  if ((str = htsmsg_get_str(msg, "id")) == NULL)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryAdd/autorecEntryUpdate: 'id' missing");
    return false;
  }

  /* Locate/create entry */
  AutoRecording &rec = m_autoRecordings[std::string(str)];
  rec.SetStringId(std::string(str));
  rec.SetDirty(false);

  /* Validate/set fields mandatory for autorecEntryAdd */

  if (!htsmsg_get_u32(msg, "enabled", &u32))
  {
    rec.SetEnabled(u32);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryAdd: 'enabled' missing");
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
      Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryAdd: 'removal' missing");
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
      Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryAdd: 'retention' missing");
      return false;
    }
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
  else if (bAdd && (m_conn.GetProtocol() >= 20))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryAdd: 'dupDetect' missing");
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
    rec.SetChannel(PVR_TIMER_ANY_CHANNEL); // an empty channel field = any channel

  if (!htsmsg_get_u32(msg, "fulltext", &u32))
  {
    rec.SetFulltext(u32);
  }

  return true;
}

bool AutoRecordings::ParseAutorecDelete(htsmsg_t *msg)
{
  const char *id;

  /* Validate/set mandatory fields */
  if ((id = htsmsg_get_str(msg, "id")) == NULL)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed autorecEntryDelete: 'id' missing");
    return false;
  }
  Logger::Log(LogLevel::LEVEL_TRACE, "delete autorec entry %s", id);

  /* Erase */
  m_autoRecordings.erase(std::string(id));

  return true;
}
