/*
 *      Copyright (C) 2005-2014 Team XBMC
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

#include <algorithm>
#include "Tvheadend.h"

#include "platform/util/util.h"
#include "platform/threads/atomics.h"

extern "C" {
#include "libhts/htsmsg_binary.h"
}

#define UPDATE(x, y)\
if ((x) != (y))\
{\
  (x) = (y);\
  update = true;\
}

using namespace std;
using namespace ADDON;
using namespace PLATFORM;

CTvheadend::CTvheadend(tvheadend::Settings settings)
  : m_settings(settings), m_dmx(m_conn), m_vfs(m_conn), 
    m_queue((size_t)-1), m_asyncState(settings.iResponseTimeout)
{
}

CTvheadend::~CTvheadend()
{
  m_conn.StopThread(-1);
  m_conn.Disconnect();
  StopThread();
}

void CTvheadend::Start ( void )
{
  CreateThread();
  m_conn.CreateThread();
}

/* **************************************************************************
 * Miscellaneous
 * *************************************************************************/

PVR_ERROR CTvheadend::GetDriveSpace ( long long *total, long long *used )
{
  int64_t s64;
  CLockObject lock(m_conn.Mutex());

  htsmsg_t *m = htsmsg_create_map();
  m = m_conn.SendAndWait("getDiskSpace", m);
  if (m == NULL)
    return PVR_ERROR_SERVER_ERROR;

  if (htsmsg_get_s64(m, "totaldiskspace", &s64))
    goto error;
  *total = s64 / 1024;

  if (htsmsg_get_s64(m, "freediskspace", &s64))
    goto error;
  *used  = *total - (s64 / 1024);

  htsmsg_destroy(m);
  return PVR_ERROR_NO_ERROR;

error:
  htsmsg_destroy(m);
  tvherror("malformed getDiskSpace response: 'totaldiskspace'/'freediskspace' missing");
  return PVR_ERROR_SERVER_ERROR;
}

CStdString CTvheadend::GetImageURL ( const char *str )
{
  if (*str != '/')
    return str;
  else
  {
    return m_conn.GetWebURL("%s", str);
  }
}

/* **************************************************************************
 * Tags
 * *************************************************************************/

int CTvheadend::GetTagCount ( void )
{
  if (!m_asyncState.WaitForState(ASYNC_DVR))
    return 0;
  
  CLockObject lock(m_mutex);
  return m_tags.size();
}

PVR_ERROR CTvheadend::GetTags ( ADDON_HANDLE handle, bool bRadio )
{
  if (!m_asyncState.WaitForState(ASYNC_DVR))
    return PVR_ERROR_FAILED;
  
  std::vector<PVR_CHANNEL_GROUP> tags;
  {
    CLockObject lock(m_mutex);
    htsp::Tags::const_iterator it;
    for (it = m_tags.begin(); it != m_tags.end(); ++it)
    {
      /* Does group contain channels of the requested type?             */
      /* Note: tvheadend groups can contain both radio and tv channels. */
      /*       Thus, one tvheadend group can 'map' to two Kodi groups.  */
      if (!it->second.ContainsChannelType(bRadio))
        continue;

      PVR_CHANNEL_GROUP tag;
      memset(&tag, 0, sizeof(tag));

      strncpy(tag.strGroupName, it->second.GetName().c_str(),
              sizeof(tag.strGroupName) - 1);
      tag.bIsRadio = bRadio;
      tag.iPosition = it->second.GetIndex();
      tags.push_back(tag);
    }
  }

  std::vector<PVR_CHANNEL_GROUP>::const_iterator it;
  for (it = tags.begin(); it != tags.end(); ++it)
  {
    /* Callback. */
    PVR->TransferChannelGroup(handle, &(*it));
  }
  
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::GetTagMembers
  ( ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group )
{
  if (!m_asyncState.WaitForState(ASYNC_DVR))
    return PVR_ERROR_FAILED;
  
  std::vector<PVR_CHANNEL_GROUP_MEMBER> gms;
  {
    CLockObject lock(m_mutex);
    vector<uint32_t>::const_iterator it;
    SChannels::const_iterator cit;
    htsp::Tags::const_iterator tit = m_tags.begin();
    while (tit != m_tags.end())
    {
      if (tit->second.GetName() == group.strGroupName)
      {
        for (it = tit->second.GetChannels().begin();
             it != tit->second.GetChannels().end(); ++it)
        {
          if ((cit = m_channels.find(*it)) != m_channels.end())
          {
            if (group.bIsRadio != cit->second.radio)
              continue;

            PVR_CHANNEL_GROUP_MEMBER gm;
            memset(&gm, 0, sizeof(PVR_CHANNEL_GROUP_MEMBER));
            strncpy(
              gm.strGroupName, group.strGroupName, sizeof(gm.strGroupName) - 1);
            gm.iChannelUniqueId = cit->second.id;
            gm.iChannelNumber   = cit->second.num;
            gms.push_back(gm);
          }
        }
        break;
      }
      ++tit;
    }
  }

  std::vector<PVR_CHANNEL_GROUP_MEMBER>::const_iterator it;
  for (it = gms.begin(); it != gms.end(); ++it)
  {
    /* Callback. */
    PVR->TransferChannelGroupMember(handle, &(*it));
  }

  return PVR_ERROR_NO_ERROR;
}

/* **************************************************************************
 * Channels
 * *************************************************************************/

int CTvheadend::GetChannelCount ( void )
{
  if (!m_asyncState.WaitForState(ASYNC_DVR))
    return 0;
  
  CLockObject lock(m_mutex);
  return m_channels.size();
}

PVR_ERROR CTvheadend::GetChannels ( ADDON_HANDLE handle, bool radio )
{
  if (!m_asyncState.WaitForState(ASYNC_DVR))
    return PVR_ERROR_FAILED;
  
  std::vector<PVR_CHANNEL> channels;
  {
    CLockObject lock(m_mutex);
    SChannels::const_iterator it;
    for (it = m_channels.begin(); it != m_channels.end(); ++it)
    {
      if (radio != it->second.radio)
        continue;

      PVR_CHANNEL chn;
      memset(&chn, 0 , sizeof(PVR_CHANNEL));

      chn.iUniqueId         = it->second.id;
      chn.bIsRadio          = it->second.radio;
      chn.iChannelNumber    = it->second.num;
      chn.iSubChannelNumber = it->second.numMinor;
      chn.iEncryptionSystem = it->second.caid;
      chn.bIsHidden         = false;
      strncpy(chn.strChannelName, it->second.name.c_str(),
              sizeof(chn.strChannelName) - 1);
      strncpy(chn.strIconPath, it->second.icon.c_str(),
              sizeof(chn.strIconPath) - 1);
      channels.push_back(chn);
    }
  }

  std::vector<PVR_CHANNEL>::const_iterator it;
  for (it = channels.begin(); it != channels.end(); ++it)
  {
    /* Callback. */
    PVR->TransferChannelEntry(handle, &(*it));
  }

  return PVR_ERROR_NO_ERROR;
}

/* **************************************************************************
 * Recordings
 * *************************************************************************/

PVR_ERROR CTvheadend::SendDvrDelete ( uint32_t id, const char *method )
{
  uint32_t u32;

  CLockObject lock(m_conn.Mutex());

  /* Build message */
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", id);

  /* Send and wait a bit longer than usual */
  if ((m = m_conn.SendAndWait(method, m,
            std::max(30000, m_settings.iResponseTimeout))) == NULL)
    return PVR_ERROR_SERVER_ERROR;

  /* Check for error */
  if (htsmsg_get_u32(m, "success", &u32))
  {
    tvherror("malformed deleteDvrEntry/cancelDvrEntry response: 'success' missing");
  }
  htsmsg_destroy(m);

  return u32 > 0  ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
}

PVR_ERROR CTvheadend::SendDvrUpdate( htsmsg_t* m )
{
  uint32_t u32;

  /* Send and Wait */
  {
    CLockObject lock(m_conn.Mutex());
    m = m_conn.SendAndWait("updateDvrEntry", m);
  }

  if (m == NULL)
    return PVR_ERROR_SERVER_ERROR;

  /* Check for error */
  if (htsmsg_get_u32(m, "success", &u32))
  {
    tvherror("malformed updateDvrEntry response: 'success' missing");
  }
  htsmsg_destroy(m);

  return u32 > 0  ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
}

int CTvheadend::GetRecordingCount ( void )
{
  if (!m_asyncState.WaitForState(ASYNC_EPG))
    return 0;
  
  int ret = 0;
  SRecordings::const_iterator rit;
  CLockObject lock(m_mutex);
  for (rit = m_recordings.begin(); rit != m_recordings.end(); ++rit)
    if (rit->second.IsRecording())
      ret++;
  return ret;
}

PVR_ERROR CTvheadend::GetRecordings ( ADDON_HANDLE handle )
{
  if (!m_asyncState.WaitForState(ASYNC_EPG))
    return PVR_ERROR_FAILED;
  
  std::vector<PVR_RECORDING> recs;
  {
    CLockObject lock(m_mutex);
    SRecordings::const_iterator rit;
    SChannels::const_iterator cit;
    char buf[128];

    for (rit = m_recordings.begin(); rit != m_recordings.end(); ++rit)
    {
      if (!rit->second.IsRecording()) continue;

      /* Setup entry */
      PVR_RECORDING rec;
      memset(&rec, 0, sizeof(rec));

      /* Channel name and icon */
      if ((cit = m_channels.find(rit->second.channel)) != m_channels.end())
      {
        strncpy(rec.strChannelName, cit->second.name.c_str(),
                sizeof(rec.strChannelName) - 1);

        strncpy(rec.strIconPath, cit->second.icon.c_str(),
                sizeof(rec.strIconPath) - 1);
      }

      /* URL ( HTSP < v7 ) */
      // TODO: do I care!

      /* ID */
      snprintf(buf, sizeof(buf), "%i", rit->second.id);
      strncpy(rec.strRecordingId, buf, sizeof(rec.strRecordingId) - 1);

      /* Title */
      strncpy(rec.strTitle, rit->second.title.c_str(), sizeof(rec.strTitle) - 1);

      /* Description */
      strncpy(rec.strPlot, rit->second.description.c_str(), sizeof(rec.strPlot) - 1);

      /* Time/Duration */
      rec.recordingTime = (time_t)rit->second.start;
      rec.iDuration     = (time_t)(rit->second.stop - rit->second.start);

      /* Priority */
      rec.iPriority = rit->second.priority;

      /* Retention */
      rec.iLifetime = rit->second.retention;

      /* Directory */
      if (rit->second.path != "")
      {
        size_t idx = rit->second.path.rfind("/");
        if (idx == 0 || idx == string::npos)
          strncpy(rec.strDirectory, "/", sizeof(rec.strDirectory) - 1);
        else
        {
          CStdString d = rit->second.path.substr(0, idx);
          if (d[0] != '/')
            d = "/" + d;
          strncpy(rec.strDirectory, d.c_str(), sizeof(rec.strDirectory) - 1);
        }
      }

      /* EPG event id */
      rec.iEpgEventId = rit->second.eventId;

      recs.push_back(rec);
    }
  }

  std::vector<PVR_RECORDING>::const_iterator it;
  for (it = recs.begin(); it != recs.end(); ++it)
  {
    /* Callback. */
    PVR->TransferRecordingEntry(handle, &(*it));
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::GetRecordingEdl
  ( const PVR_RECORDING &rec, PVR_EDL_ENTRY edl[], int *num )
{
  /* Not supported */
  if (m_conn.GetProtocol() < 12)
    return PVR_ERROR_NOT_IMPLEMENTED;
  
  htsmsg_t *list;
  htsmsg_field_t *f;
  int idx;
  
  /* Build request */
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", atoi(rec.strRecordingId));
  
  tvhdebug("dvr get cutpoints id=%s", rec.strRecordingId);

  /* Send and Wait */
  {
    CLockObject lock(m_conn.Mutex());
    
    if ((m = m_conn.SendAndWait("getDvrCutpoints", m)) == NULL)
      return PVR_ERROR_SERVER_ERROR;
  }

  /* Check for optional "cutpoints" reply message field */
  if (!(list = htsmsg_get_list(m, "cutpoints")))
  {
    *num = 0;
    htsmsg_destroy(m);
    return PVR_ERROR_NO_ERROR;
  }

  /* Process */
  idx = 0;
  HTSMSG_FOREACH(f, list)
  {
    uint32_t start, end, type;

    if (f->hmf_type != HMF_MAP)
      continue;
  
    /* Full */
    if (idx >= *num)
      break;

    /* Get fields */
    if (htsmsg_get_u32(&f->hmf_msg, "start", &start) ||
        htsmsg_get_u32(&f->hmf_msg, "end",   &end)   ||
        htsmsg_get_u32(&f->hmf_msg, "type",  &type))
    {
      tvherror("malformed getDvrCutpoints response: invalid EDL entry, will ignore");
      continue;
    }

    /* Build entry */
    edl[idx].start = start;
    edl[idx].end   = end;
    switch (type)
    {
      case DVR_ACTION_TYPE_CUT:
        edl[idx].type = PVR_EDL_TYPE_CUT;
        break;
      case DVR_ACTION_TYPE_MUTE:
        edl[idx].type = PVR_EDL_TYPE_MUTE;
        break;
      case DVR_ACTION_TYPE_SCENE:
        edl[idx].type = PVR_EDL_TYPE_SCENE;
        break;
      case DVR_ACTION_TYPE_COMBREAK:
      default:
        edl[idx].type = PVR_EDL_TYPE_COMBREAK;
        break;
    }
    idx++;
      
    tvhdebug("edl start:%d end:%d action:%d", start, end, type);
  }
  
  *num = idx;
  htsmsg_destroy(m);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::DeleteRecording ( const PVR_RECORDING &rec )
{
  return SendDvrDelete(atoi(rec.strRecordingId), "deleteDvrEntry");
}

PVR_ERROR CTvheadend::RenameRecording ( const PVR_RECORDING &rec )
{
  /* Build message */
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "id",     atoi(rec.strRecordingId));
  htsmsg_add_str(m, "title",  rec.strTitle);

  return SendDvrUpdate(m);
}

int CTvheadend::GetTimerCount ( void )
{
  if (!m_asyncState.WaitForState(ASYNC_EPG))
    return 0;
  
  int ret = 0;
  SRecordings::const_iterator rit;
  CLockObject lock(m_mutex);
  for (rit = m_recordings.begin(); rit != m_recordings.end(); ++rit)
    if (rit->second.IsTimer())
      ret++;
  return ret;
}

PVR_ERROR CTvheadend::GetTimers ( ADDON_HANDLE handle )
{
  if (!m_asyncState.WaitForState(ASYNC_EPG))
    return PVR_ERROR_FAILED;
  
  std::vector<PVR_TIMER> timers;
  {
    CLockObject lock(m_mutex);
    SRecordings::const_iterator rit;

    for (rit = m_recordings.begin(); rit != m_recordings.end(); ++rit)
    {
      if (!rit->second.IsTimer()) continue;

      /* Setup entry */
      PVR_TIMER tmr;
      memset(&tmr, 0, sizeof(tmr));

      tmr.iClientIndex      = rit->second.id;
      tmr.iClientChannelUid = rit->second.channel;
      tmr.startTime         = (time_t)rit->second.start;
      tmr.endTime           = (time_t)rit->second.stop;
      strncpy(tmr.strTitle, rit->second.title.c_str(), 
              sizeof(tmr.strTitle) - 1);
      strncpy(tmr.strSummary, rit->second.description.c_str(),
              sizeof(tmr.strSummary) - 1);
      tmr.state             = rit->second.state;
      tmr.iPriority         = rit->second.priority;
      tmr.iLifetime         = rit->second.retention;
      tmr.bIsRepeating      = false; // unused
      tmr.firstDay          = 0;     // unused
      tmr.iWeekdays         = 0;     // unused
      tmr.iEpgUid           = 0;     // unused
      tmr.iMarginStart      = rit->second.startExtra;
      tmr.iMarginEnd        = rit->second.stopExtra;
      tmr.iGenreType        = 0;     // unused
      tmr.iGenreSubType     = 0;     // unused

      timers.push_back(tmr);
    }
  }

  std::vector<PVR_TIMER>::const_iterator it;
  for (it = timers.begin(); it != timers.end(); ++it)
  {
    /* Callback. */
    PVR->TransferTimerEntry(handle, &(*it));
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::AddTimer ( const PVR_TIMER &timer )
{
  uint32_t u32;
  dvr_prio_t prio;

  if (timer.bIsRepeating && timer.iWeekdays)
  {
    if (m_conn.GetProtocol() >= 18)
      return AddTimeRecording(timer);
    else
      return PVR_ERROR_NOT_IMPLEMENTED;
  }

  /* Build message */
  htsmsg_t *m = htsmsg_create_map();
  if (timer.iEpgUid > 0)
  {
    htsmsg_add_u32(m, "eventId",      timer.iEpgUid);
  }
  else
  {
    htsmsg_add_str(m, "title",        timer.strTitle);
    htsmsg_add_s64(m, "start",        timer.startTime);
    htsmsg_add_s64(m, "stop",         timer.endTime);
    htsmsg_add_u32(m, "channelId",    timer.iClientChannelUid);
    htsmsg_add_str(m, "description",  timer.strSummary);
  }

  htsmsg_add_s64(m, "startExtra", timer.iMarginStart);
  htsmsg_add_s64(m, "stopExtra",  timer.iMarginEnd);

  if (m_conn.GetProtocol() > 12)
    htsmsg_add_u32(m, "retention", timer.iLifetime);

  /* Priority */
  if (timer.iPriority > 80)
    prio = DVR_PRIO_IMPORTANT;
  else if (timer.iPriority > 60)
    prio = DVR_PRIO_HIGH;
  else if (timer.iPriority > 40)
    prio = DVR_PRIO_NORMAL;
  else if (timer.iPriority > 20)
    prio = DVR_PRIO_LOW;
  else
    prio = DVR_PRIO_UNIMPORTANT;

  htsmsg_add_u32(m, "priority", (int)prio);

  /* Send and Wait */
  {
    CLockObject lock(m_conn.Mutex());
    m = m_conn.SendAndWait("addDvrEntry", m);
  }

  if (m == NULL)
    return PVR_ERROR_SERVER_ERROR;

  /* Check for error */
  if (htsmsg_get_u32(m, "success", &u32))
  {
    tvherror("malformed addDvrEntry response: 'success' missing");
  }
  htsmsg_destroy(m);

  return u32 > 0  ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
}

PVR_ERROR CTvheadend::DeleteTimer
  ( const PVR_TIMER &timer, bool _unused(force) )
{
  return SendDvrDelete(timer.iClientIndex, "cancelDvrEntry");
}

PVR_ERROR CTvheadend::UpdateTimer ( const PVR_TIMER &timer )
{
  /* Build message */
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "id",           timer.iClientIndex);
  htsmsg_add_str(m, "title",        timer.strTitle);
  htsmsg_add_s64(m, "start",        timer.startTime);
  htsmsg_add_s64(m, "stop",         timer.endTime);
  htsmsg_add_str(m, "description",  timer.strSummary);
  htsmsg_add_s64(m, "startExtra",   timer.iMarginStart);
  htsmsg_add_s64(m, "stopExtra",    timer.iMarginEnd);

  if (m_conn.GetProtocol() > 12)
  {
    dvr_prio_t prio;

    htsmsg_add_u32(m, "retention", timer.iLifetime);

    /* Priority */
    if (timer.iPriority > 80)
      prio = DVR_PRIO_IMPORTANT;
    else if (timer.iPriority > 60)
      prio = DVR_PRIO_HIGH;
    else if (timer.iPriority > 40)
      prio = DVR_PRIO_NORMAL;
    else if (timer.iPriority > 20)
      prio = DVR_PRIO_LOW;
    else
      prio = DVR_PRIO_UNIMPORTANT;

    htsmsg_add_u32(m, "priority",   (int)prio);
  }

  return SendDvrUpdate(m);
}

PVR_ERROR CTvheadend::AddTimeRecording ( const PVR_TIMER &timer )
{
  uint32_t u32;
  dvr_prio_t prio;

  /* Build message */
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "daysOfWeek",   timer.iWeekdays);
  htsmsg_add_str(m, "title",        timer.strTitle);
  htsmsg_add_str(m, "name",         timer.strTitle);
  htsmsg_add_u32(m, "channelId",    timer.iClientChannelUid);
  htsmsg_add_str(m, "description",  timer.strSummary);
  htsmsg_add_str(m, "comment",      "Created by Kodi Media Center");

  /* Convert start and stop time to time after midnight */
  struct tm *tmi;
  tmi = localtime(&timer.startTime);
  htsmsg_add_u32(m, "start",        (tmi->tm_hour*60 + tmi->tm_min));
  tmi = localtime(&timer.endTime);
  htsmsg_add_u32(m, "stop",         (tmi->tm_hour*60 + tmi->tm_min));

  /* Retention */
  if (m_conn.GetProtocol() > 12)
    htsmsg_add_u32(m, "retention", timer.iLifetime);

  /* Priority */
  if (timer.iPriority > 80)
    prio = DVR_PRIO_IMPORTANT;
  else if (timer.iPriority > 60)
    prio = DVR_PRIO_HIGH;
  else if (timer.iPriority > 40)
    prio = DVR_PRIO_NORMAL;
  else if (timer.iPriority > 20)
    prio = DVR_PRIO_LOW;
  else
    prio = DVR_PRIO_UNIMPORTANT;

  htsmsg_add_u32(m, "priority", (int)prio);

  /* Send and Wait */
  CLockObject lock(m_conn.Mutex());
  m = m_conn.SendAndWait("addTimerecEntry", m);

  if (m == NULL)
    return PVR_ERROR_SERVER_ERROR;

  /* Check for error */
  if (htsmsg_get_u32(m, "success", &u32))
    tvherror("malformed addTimerecEntry response: 'success' missing");

  htsmsg_destroy(m);

  return u32 > 0  ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
}

/* **************************************************************************
 * EPG
 * *************************************************************************/

/* Transfer schedule to XBMC */
void CTvheadend::TransferEvent
  ( ADDON_HANDLE handle, const SEvent &event )
{
  /* Build */
  EPG_TAG epg;
  memset(&epg, 0, sizeof(EPG_TAG));
  epg.iUniqueBroadcastId  = event.id;
  epg.strTitle            = event.title.c_str();
  epg.iChannelNumber      = event.channel;
  epg.startTime           = event.start;
  epg.endTime             = event.stop;
  epg.strPlotOutline      = event.summary.c_str();
  epg.strPlot             = event.desc.c_str();
  epg.strOriginalTitle    = NULL; /* not supported by tvh */
  epg.strCast             = NULL; /* not supported by tvh */
  epg.strDirector         = NULL; /* not supported by tvh */
  epg.strWriter           = NULL; /* not supported by tvh */
  epg.iYear               = 0;    /* not supported by tvh */
  epg.strIMDBNumber       = NULL; /* not supported by tvh */
  epg.strIconPath         = event.image.c_str();
  epg.iGenreType          = event.content & 0xF0;
  epg.iGenreSubType       = event.content & 0x0F;
  epg.strGenreDescription = NULL; /* not supported by tvh */
  epg.firstAired          = event.aired;
  epg.iParentalRating     = event.age;
  epg.iStarRating         = event.stars;
  epg.bNotify             = false; /* not supported by tvh */
  epg.iSeriesNumber       = event.season;
  epg.iEpisodeNumber      = event.episode;
  epg.iEpisodePartNumber  = event.part;
  epg.strEpisodeName      = event.subtitle.c_str();

  /* Callback. */
  PVR->TransferEpgEntry(handle, &epg);
}

PVR_ERROR CTvheadend::GetEpg
  ( ADDON_HANDLE handle, const PVR_CHANNEL &chn, time_t start, time_t end )
{
  SSchedules::const_iterator sit;
  SEvents::const_iterator eit;
  htsmsg_field_t *f;
  int n = 0;

  tvhtrace("get epg channel %d start %ld stop %ld", chn.iUniqueId,
           (long long)start, (long long)end);

  /* Async transfer */
  if (m_settings.bAsyncEpg)
  {
    if (!m_asyncState.WaitForState(ASYNC_DONE))
      return PVR_ERROR_FAILED;
    
    std::vector<SEvent> events;
    {
      CLockObject lock(m_mutex);
      sit = m_schedules.find(chn.iUniqueId);
      if (sit != m_schedules.end())
      {
        for (eit = sit->second.events.begin();
            eit != sit->second.events.end(); ++eit)
        {
          if (eit->second.start    > end)   continue;
          if (eit->second.stop     < start) continue;

          events.push_back(eit->second);
          ++n;
        }
      }
    }

    std::vector<SEvent>::const_iterator it;
    for (it = events.begin(); it != events.end(); ++it)
    {
      /* Callback. */
      TransferEvent(handle, *it);
    }

  /* Synchronous transfer */
  }
  else
  {
    /* Build message */
    htsmsg_t *msg = htsmsg_create_map();
    htsmsg_add_u32(msg, "channelId", chn.iUniqueId);
    htsmsg_add_s64(msg, "maxTime",   end);

    /* Send and Wait */
    {
      CLockObject lock(m_conn.Mutex());
      
      if ((msg = m_conn.SendAndWait0("getEvents", msg)) == NULL)
        return PVR_ERROR_SERVER_ERROR;
    }

    /* Process */
    htsmsg_t *l;
    
    if (!(l = htsmsg_get_list(msg, "events")))
    {
      htsmsg_destroy(msg);
      tvherror("malformed getEvents response: 'events' missing");
      return PVR_ERROR_SERVER_ERROR;
    }
    HTSMSG_FOREACH(f, l)
    {
      SEvent event;
      if (f->hmf_type == HMF_MAP)
      {
        if (ParseEvent(&f->hmf_msg, true, event))
        {
          /* Callback. */
          TransferEvent(handle, event);
          ++n;
        }
      }
    }
    htsmsg_destroy(msg);
  }

  tvhtrace("get epg channel %d events %d", chn.iUniqueId, n);

  return PVR_ERROR_NO_ERROR;
}

/* **************************************************************************
 * Connection
 * *************************************************************************/

void CTvheadend::Disconnected ( void )
{
  m_asyncState.SetState(ASYNC_NONE);
}

bool CTvheadend::Connected ( void )
{
  htsmsg_t *msg;
  htsp::Tags::iterator tit;
  SChannels::iterator cit;
  SRecordings::iterator rit;
  SSchedules::iterator sit;
  SEvents::iterator eit;

  /* Rebuild state */
  m_dmx.Connected();
  m_vfs.Connected();

  /* Flag all async fields in case they've been deleted */
  for (cit = m_channels.begin(); cit != m_channels.end(); ++cit)
    cit->second.del = true;
  for (tit = m_tags.begin(); tit != m_tags.end(); ++tit)
    tit->second.SetDirty(true);
  for (rit = m_recordings.begin(); rit != m_recordings.end(); ++rit)
    rit->second.del = true;
  for (sit = m_schedules.begin(); sit != m_schedules.end(); ++sit)
  {
    sit->second.del = true;

    for (eit = sit->second.events.begin(); eit != sit->second.events.end(); ++eit)
      eit->second.del = true;
  }

  /* Request Async data */
  m_asyncState.SetState(ASYNC_NONE);
  
  msg = htsmsg_create_map();
  htsmsg_add_u32(msg, "epg", m_settings.bAsyncEpg);
  //htsmsg_add_u32(msg, "epgMaxTime", 0);
  //htsmsg_add_s64(msg, "lastUpdate", 0);
  if ((msg = m_conn.SendAndWait0("enableAsyncMetadata", msg)) == NULL)
    return false;

  htsmsg_destroy(msg);
  tvhdebug("async updates requested");

  return true;
}

/* **************************************************************************
 * Message handling
 * *************************************************************************/

bool CTvheadend::ProcessMessage ( const char *method, htsmsg_t *msg )
{
  /* Demuxer */
  if (m_dmx.ProcessMessage(method, msg))
    return true;

  /* Store */
  m_queue.Push(CHTSPMessage(method, msg));
  return false;
}
  
void* CTvheadend::Process ( void )
{
  CHTSPMessage msg;
  const char *method;

  while (!IsStopped())
  {
    /* Check Q */
    // this is a bit horrible, but meh
    if (!m_queue.Pop(msg, 2000))
      continue;
    if (!msg.m_msg)
      continue;
    method = msg.m_method.c_str();
    
    /* Scope lock for processing */
    {
      CLockObject lock(m_mutex);

      /* Channels */
      if (!strcmp("channelAdd", method))
        ParseChannelAddOrUpdate(msg.m_msg, true);
      else if (!strcmp("channelUpdate", method))
        ParseChannelAddOrUpdate(msg.m_msg, false);
      else if (!strcmp("channelDelete", method))
        ParseChannelDelete(msg.m_msg);

      /* Channel Tags (aka channel groups)*/
      else if (!strcmp("tagAdd", method))
        ParseTagAddOrUpdate(msg.m_msg, true);
      else if (!strcmp("tagUpdate", method))
        ParseTagAddOrUpdate(msg.m_msg, false);
      else if (!strcmp("tagDelete", method))
        ParseTagDelete(msg.m_msg);

      /* Recordings */
      else if (!strcmp("dvrEntryAdd", method))
        ParseRecordingAddOrUpdate(msg.m_msg, true);
      else if (!strcmp("dvrEntryUpdate", method))
        ParseRecordingAddOrUpdate(msg.m_msg, false);
      else if (!strcmp("dvrEntryDelete", method))
        ParseRecordingDelete(msg.m_msg);

      /* EPG */
      else if (!strcmp("eventAdd", method))
        ParseEventAddOrUpdate(msg.m_msg, true);
      else if (!strcmp("eventUpdate", method))
        ParseEventAddOrUpdate(msg.m_msg, false);
      else if (!strcmp("eventDelete", method))
        ParseEventDelete(msg.m_msg);

      /* ASync complete */
      else if (!strcmp("initialSyncCompleted", method))
        SyncCompleted();

      /* Unknown */
      else  
        tvhdebug("unhandled message [%s]", method);
    }
  
    /* Manual delete rather than waiting */
    htsmsg_destroy(msg.m_msg);
    msg.m_msg = NULL;

    /* Process events
     * Note: due to potential deadly embrace this must be done without the
     *       m_mutex held!
     */
    SHTSPEventList::const_iterator it;
    for (it = m_events.begin(); it != m_events.end(); ++it)
    {
      switch (it->m_type)
      {
        case HTSP_EVENT_TAG_UPDATE:
          PVR->TriggerChannelGroupsUpdate();
          break;
        case HTSP_EVENT_CHN_UPDATE:
          PVR->TriggerChannelUpdate();
          break;
        case HTSP_EVENT_REC_UPDATE:
          PVR->TriggerTimerUpdate();
          PVR->TriggerRecordingUpdate();
          break;
        case HTSP_EVENT_EPG_UPDATE:
          PVR->TriggerEpgUpdate(it->m_idx);
          break;
        case HTSP_EVENT_NONE:
          break;
      }
    }
    m_events.clear();
  }

  /* Local */
  return NULL;
}

void CTvheadend::SyncCompleted ( void )
{
  /* The complete calls are probably redundant, but its a safety feature */
  SyncChannelsCompleted();
  SyncDvrCompleted();
  SyncEpgCompleted();
  m_asyncState.SetState(ASYNC_DONE);
}

void CTvheadend::SyncChannelsCompleted ( void )
{
  /* Already done */
  if (m_asyncState.GetState() > ASYNC_CHN)
    return;

  bool update;
  SChannels::iterator   cit = m_channels.begin();
  htsp::Tags::iterator  tit = m_tags.begin();

  /* Tags */
  update = false;
  while (tit != m_tags.end())
  {
    if (tit->second.IsDirty())
    {
      update = true;
      m_tags.erase(tit++);
    }
    else
      ++tit;
  }
  TriggerChannelGroupsUpdate();
  if (update)
    tvhinfo("tags updated");

  /* Channels */
  update = false;
  while (cit != m_channels.end())
  {
    if (cit->second.del)
    {
      update = true;
      m_channels.erase(cit++);
    }
    else
      ++cit;
  }
  TriggerChannelUpdate();
  if (update)
    tvhinfo("channels updated");
  
  /* Next */
  m_asyncState.SetState(ASYNC_DVR);
}

void CTvheadend::SyncDvrCompleted ( void )
{
  /* Done */
  if (m_asyncState.GetState() > ASYNC_DVR)
    return;

  bool update;
  SRecordings::iterator rit = m_recordings.begin();

  /* Recordings */
  update = false;
  while (rit != m_recordings.end())
  {
    if (rit->second.del)
    {
      update = true;
      m_recordings.erase(rit++);
    }
    else
      ++rit;
  }
  TriggerRecordingUpdate();
  TriggerTimerUpdate();
  if (update)
    tvhinfo("recordings updated");

  /* Next */
  m_asyncState.SetState(ASYNC_EPG);
}

void CTvheadend::SyncEpgCompleted ( void )
{
  /* Done */
  if (!m_settings.bAsyncEpg || m_asyncState.GetState() > ASYNC_EPG)
    return;
  
  bool update;
  SSchedules::iterator  sit = m_schedules.begin();
  SEvents::iterator     eit;

  /* Events */
  update = false;
  while (sit != m_schedules.end())
  {
    uint32_t channelId = sit->second.channel;
    
    if (sit->second.del)
    {
      update = true;
      m_schedules.erase(sit++);
    }
    else
    {
      eit = sit->second.events.begin();
      while (eit != sit->second.events.end())
      {
        if (eit->second.del)
        {
          update = true;
          sit->second.events.erase(eit++);
        }
        else
          ++eit;
      }
      ++sit;
    }

    TriggerEpgUpdate(channelId);
  }

  if (update)
    tvhinfo("epg updated");
}

void CTvheadend::ParseTagAddOrUpdate ( htsmsg_t *msg, bool bAdd )
{
  uint32_t u32;
  const char *str;
  htsmsg_t *list;

  /* Validate */
  if (htsmsg_get_u32(msg, "tagId", &u32))
  {
    tvherror("malformed tagAdd/tagUpdate: 'tagId' missing");
    return;
  }

  /* Locate object */
  htsp::Tag &existingTag = m_tags[u32];
  existingTag.SetDirty(false);
  
  /* Create new object */
  htsp::Tag tag(u32);

  /* Index */
  if (!htsmsg_get_u32(msg, "tagIndex", &u32))
    tag.SetIndex(u32);

  /* Name */
  if ((str = htsmsg_get_str(msg, "tagName")) != NULL)
    tag.SetName(str);
  else if (bAdd)
  {
    tvherror("malformed tagAdd: 'tagName' missing");
    return;
  }

  /* Icon */
  if ((str = htsmsg_get_str(msg, "tagIcon")) != NULL)
    tag.SetIcon(GetImageURL(str));

  /* Members */
  if ((list = htsmsg_get_list(msg, "members")) != NULL)
  {
    htsmsg_field_t *f;
    HTSMSG_FOREACH(f, list)
    {
      if (f->hmf_type != HMF_S64) continue;
      tag.GetChannels().push_back((int)f->hmf_s64);
    }
  }

  /* Update */
  if (existingTag != tag)
  {
    existingTag = tag;
    tvhdebug("tag updated id:%u, name:%s",
              existingTag.GetId(), existingTag.GetName().c_str());
    if (m_asyncState.GetState() > ASYNC_CHN)
      TriggerChannelGroupsUpdate();
  }
}

void CTvheadend::ParseTagDelete ( htsmsg_t *msg )
{
  uint32_t u32;

  /* Validate */
  if (htsmsg_get_u32(msg, "tagId", &u32))
  {
    tvherror("malformed tagDelete: 'tagId' missing");
    return;
  }
  tvhdebug("delete tag %u", u32);
  
  /* Erase */
  m_tags.erase(u32);
  TriggerChannelGroupsUpdate();
}

void CTvheadend::ParseChannelAddOrUpdate ( htsmsg_t *msg, bool bAdd )
{
  bool update = false;
  uint32_t u32;
  const char *str;
  htsmsg_t *list;

  /* Validate */
  if (htsmsg_get_u32(msg, "channelId", &u32))
  {
    tvherror("malformed channelAdd/channelUpdate: 'channelId' missing");
    return;
  }

  /* Locate channel object */
  SChannel &channel = m_channels[u32];
  channel.id  = u32;
  channel.del = false;

  /* Channel name */
  if ((str = htsmsg_get_str(msg, "channelName")) != NULL)
  {
    UPDATE(channel.name, str);
  }
  else if (bAdd)
  {
    tvherror("malformed channelAdd: 'channelName' missing");
    return;
  }

  /* Channel number */
  if (!htsmsg_get_u32(msg, "channelNumber", &u32))
  {
    if (!u32)
      u32 = GetNextUnnumberedChannelNumber();
    UPDATE(channel.num, u32);
  }
  else if (bAdd)
  {
    tvherror("malformed channelAdd: 'channelNumber' missing");
    return;
  }
  else if (!channel.num)
  {
    UPDATE(channel.num, GetNextUnnumberedChannelNumber());
  }
  
  /* ATSC subchannel number */
  if (!htsmsg_get_u32(msg, "channelNumberMinor", &u32))
  {
    UPDATE(channel.numMinor, u32);
  }

  /* Channel icon */
  if ((str = htsmsg_get_str(msg, "channelIcon")) != NULL)
  {
    CStdString url = GetImageURL(str);
    UPDATE(channel.icon, url);
  }

  /* Services */
  if ((list = htsmsg_get_list(msg, "services")) != NULL)
  {
    htsmsg_field_t *f;
    uint32_t caid  = 0;
    bool     radio = false;
    HTSMSG_FOREACH(f, list)
    {
      if (f->hmf_type != HMF_MAP)
        continue;

      /* Radio? */
      if ((str = htsmsg_get_str(&f->hmf_msg, "type")) != NULL)
      {
        if (!strcmp(str, "Radio"))
          radio = true;
      }

      /* CAID */
      if (caid == 0)
        htsmsg_get_u32(&f->hmf_msg, "caid", &caid);
    }
    UPDATE(channel.radio, radio);
    UPDATE(channel.caid,  caid);
  }
  

  /* Update Kodi */
  if (update) {
    tvhdebug("channel update id:%u, name:%s",
              channel.id, channel.name.c_str());
    if (m_asyncState.GetState() > ASYNC_CHN)
      TriggerChannelUpdate();
  }
}

void CTvheadend::ParseChannelDelete ( htsmsg_t *msg )
{
  uint32_t u32;

  /* Validate */
  if (htsmsg_get_u32(msg, "channelId", &u32))
  {
    tvherror("malformed channelDelete: 'channelId' missing");
    return;
  }
  tvhdebug("delete channel %u", u32);
  
  /* Erase */
  m_channels.erase(u32);
  TriggerChannelUpdate();
}

void CTvheadend::ParseRecordingAddOrUpdate ( htsmsg_t *msg, bool bAdd )
{
  bool update = false;
  const char *state, *str;
  uint32_t id, channel, eventId, retention, priority;
  int64_t start, stop, startExtra, stopExtra;

  /* Channels must be complete */
  SyncChannelsCompleted();

  /* Validate */
  if (htsmsg_get_u32(msg, "id", &id))
  {
    tvherror("malformed dvrEntryAdd/dvrEntryUpdate: 'id' missing");
    return;
  }

  if (htsmsg_get_u32(msg, "channel", &channel) && bAdd)
  {
    tvherror("malformed dvrEntryAdd: 'channel' missing");
    return;
  }

  if (htsmsg_get_s64(msg, "start", &start) && bAdd)
  {
    tvherror("malformed dvrEntryAdd: 'start' missing");
    return;
  }

  if (htsmsg_get_s64(msg, "stop", &stop) && bAdd)
  {
    tvherror("malformed dvrEntryAdd: 'stop' missing");
    return;
  }

  if (((state = htsmsg_get_str(msg, "state")) == NULL) && bAdd)
  {
    tvherror("malformed dvrEntryAdd: 'state' missing");
    return;
  }

  /* Get entry */
  SRecording &rec = m_recordings[id];
  rec.id  = id;
  rec.del = false;
  UPDATE(rec.channel, channel);
  UPDATE(rec.start,   start);
  UPDATE(rec.stop,    stop);

  if (!htsmsg_get_s64(msg, "startExtra", &startExtra))
  {
    UPDATE(rec.startExtra, startExtra);
  }
  else if (bAdd && (m_conn.GetProtocol() > 12))
  {
    tvherror("malformed dvrEntryAdd: 'startExtra' missing");
    return;
  }

  if (!htsmsg_get_s64(msg, "stopExtra", &stopExtra))
  {
    UPDATE(rec.stopExtra,  stopExtra);
  }
  else if (bAdd && (m_conn.GetProtocol() > 12))
  {
    tvherror("malformed dvrEntryAdd: 'stopExtra' missing");
    return;
  }

  if (!htsmsg_get_u32(msg, "retention", &retention))
  {
    UPDATE(rec.retention, retention);
  }
  else if (bAdd)
  {
    tvherror("malformed dvrEntryAdd: 'retention' missing");
    return;
  }

  if (!htsmsg_get_u32(msg, "priority", &priority))
  {
    switch (priority)
    {
      case DVR_PRIO_IMPORTANT:
        UPDATE(rec.priority, 100);
        break;
      case DVR_PRIO_HIGH:
        UPDATE(rec.priority, 75);
        break;
      case DVR_PRIO_NORMAL:
        UPDATE(rec.priority, 50);
        break;
      case DVR_PRIO_LOW:
        UPDATE(rec.priority, 25);
        break;
      case DVR_PRIO_UNIMPORTANT:
        UPDATE(rec.priority, 0);
        break;
      default:
        tvherror("malformed dvrEntryAdd/dvrEntryUpdate: unknown priority value");
        return;
    }
  }
  else if (bAdd && (m_conn.GetProtocol() > 12))
  {
    tvherror("malformed dvrEntryAdd: 'priority' missing");
    return;
  }

  if (state != NULL)
  {
    /* Parse state */
    if      (strstr(state, "scheduled") != NULL)
    {
      UPDATE(rec.state, PVR_TIMER_STATE_SCHEDULED);
    }
    else if (strstr(state, "recording") != NULL)
    {
      UPDATE(rec.state, PVR_TIMER_STATE_RECORDING);
    }
    else if (strstr(state, "completed") != NULL)
    {
      UPDATE(rec.state, PVR_TIMER_STATE_COMPLETED);
    }
    else if (strstr(state, "missed") != NULL)
    {
      UPDATE(rec.state, PVR_TIMER_STATE_ERROR);
    }
    else if (strstr(state, "invalid")   != NULL)
    {
      UPDATE(rec.state, PVR_TIMER_STATE_ERROR);
    }
  }

  /* Add optional fields */
  if (!htsmsg_get_u32(msg, "eventId", &eventId))
  {
    UPDATE(rec.eventId, eventId);
  }
  if ((str = htsmsg_get_str(msg, "title")) != NULL)
  {
    UPDATE(rec.title, str);
  }
  if ((str = htsmsg_get_str(msg, "path")) != NULL)
  {
    UPDATE(rec.path, str);
  }
  if ((str = htsmsg_get_str(msg, "description")) != NULL)
  {
    UPDATE(rec.description, str);
  }
  else if ((str = htsmsg_get_str(msg, "summary")) != NULL)
  {
    UPDATE(rec.description, str);
  }
  if ((str = htsmsg_get_str(msg, "autorecId")) != NULL)
  {
    UPDATE(rec.autorecId, str);
  }
  if ((str = htsmsg_get_str(msg, "timerecId")) != NULL)
  {
    UPDATE(rec.timerecId, str);
  }

  /* Error */
  if ((str = htsmsg_get_str(msg, "error")) != NULL)
  {
    if (!strcmp(str, "300"))
    {
      UPDATE(rec.state, PVR_TIMER_STATE_ABORTED);
    }
    else if (strstr(str, "missing") != NULL)
    {
      UPDATE(rec.state, PVR_TIMER_STATE_ERROR);
    }
    else
    {
      UPDATE(rec.error, str);
    }
  }
  
  /* Update */
  if (update)
  {
    std::string error = rec.error.empty() ? "none" : rec.error;
    
    tvhdebug("recording id:%d, state:%s, title:%s, desc:%s, error:%s",
             rec.id, state, rec.title.c_str(), rec.description.c_str(),
             error.c_str());

    if (m_asyncState.GetState() > ASYNC_DVR)
    {
      TriggerTimerUpdate();
      if (rec.state == PVR_TIMER_STATE_RECORDING)
        TriggerRecordingUpdate();
    }
  }
}

void CTvheadend::ParseRecordingDelete ( htsmsg_t *msg )
{
  uint32_t u32;

  /* Validate */
  if (htsmsg_get_u32(msg, "id", &u32))
  {
    tvherror("malformed dvrEntryDelete: 'id' missing");
    return;
  }
  tvhdebug("delete recording %u", u32);
  
  /* Erase */
  m_recordings.erase(u32);

  /* Update */
  TriggerTimerUpdate();
  TriggerRecordingUpdate();
}

bool CTvheadend::ParseEvent ( htsmsg_t *msg, bool bAdd, SEvent &evt )
{
  const char *str;
  uint32_t u32, id, channel;
  int64_t s64, start, stop;

  /* Recordings complete */
  SyncDvrCompleted();

  /* Validate */
  if (htsmsg_get_u32(msg, "eventId", &id))
  {
    tvherror("malformed eventAdd/eventUpdate: 'eventId' missing");
    return false;
  }

  if (htsmsg_get_u32(msg, "channelId", &channel) && bAdd)
  {
    tvherror("malformed eventAdd: 'channelId' missing");
    return false;
  }

  if (htsmsg_get_s64(msg, "start", &start) && bAdd)
  {
    tvherror("malformed eventAdd: 'start' missing");
    return false;
  }

  if (htsmsg_get_s64(msg, "stop", &stop) && bAdd)
  {
    tvherror("malformed eventAdd: 'stop' missing");
    return false;
  }

  evt.id      = id;
  evt.channel = channel;
  evt.start   = (time_t)start;
  evt.stop    = (time_t)stop;

  /* Add optional fields */
  if ((str = htsmsg_get_str(msg, "title")) != NULL)
    evt.title   = str;
  if ((str = htsmsg_get_str(msg, "subtitle")) != NULL)
    evt.subtitle   = str;
  if ((str = htsmsg_get_str(msg, "summary")) != NULL)
    evt.summary  = str;
  if ((str = htsmsg_get_str(msg, "description")) != NULL)
    evt.desc     = str;
  if ((str = htsmsg_get_str(msg, "image")) != NULL)
    evt.image   = str;
  if (!htsmsg_get_u32(msg, "nextEventId", &u32))
    evt.next    = u32;
  if (!htsmsg_get_u32(msg, "contentType", &u32))
    evt.content = u32;
  if (!htsmsg_get_u32(msg, "starRating", &u32))
    evt.stars   = u32;
  if (!htsmsg_get_u32(msg, "ageRating", &u32))
    evt.age     = u32;
  if (!htsmsg_get_s64(msg, "firstAired", &s64))
    evt.aired   = (time_t)s64;
  if (!htsmsg_get_u32(msg, "seasonNumber", &u32))
    evt.season  = u32;
  if (!htsmsg_get_u32(msg, "episodeNumber", &u32))
    evt.episode = u32;
  if (!htsmsg_get_u32(msg, "partNumber", &u32))
    evt.part    = u32;

  /* Add optional recording link */
  for (SRecordings::const_iterator it = m_recordings.begin(); it != m_recordings.end(); ++it)
  {
    if (it->second.eventId == evt.id)
    {
      evt.recordingId = evt.id;
      break;
    }
  }
  
  return true;
}

void CTvheadend::ParseEventAddOrUpdate ( htsmsg_t *msg, bool bAdd )
{
  bool update = false;
  SEvent tmp;

  /* Parse */
  if (!ParseEvent(msg, bAdd, tmp))
    return;

  /* Get event handle */
  SSchedule &sched = m_schedules[tmp.channel];
  SEvent    &evt   = sched.events[tmp.id];
  sched.channel    = tmp.channel;
  evt.id           = tmp.id;
  evt.del          = false;
  
  /* Store */
  UPDATE(evt.title,    tmp.title);
  UPDATE(evt.subtitle, tmp.subtitle);
  UPDATE(evt.start,    tmp.start);
  UPDATE(evt.stop,     tmp.stop);
  UPDATE(evt.channel,  tmp.channel);
  UPDATE(evt.summary,  tmp.summary);
  UPDATE(evt.desc,     tmp.desc);
  UPDATE(evt.image,    tmp.image);
  UPDATE(evt.next,     tmp.next);
  UPDATE(evt.content,  tmp.content);
  UPDATE(evt.stars,    tmp.stars);
  UPDATE(evt.age,      tmp.age);
  UPDATE(evt.aired,    tmp.aired);

  /* Update */
  if (update)
  {
    tvhtrace("event id:%d channel:%d start:%d stop:%d title:%s desc:%s",
             evt.id, evt.channel, (int)evt.start, (int)evt.stop,
             evt.title.c_str(), evt.desc.c_str());

    if (m_asyncState.GetState() > ASYNC_EPG)
      TriggerEpgUpdate(tmp.channel);
  }
}

void CTvheadend::ParseEventDelete ( htsmsg_t *msg )
{
  uint32_t u32;
  
  /* Validate */
  if (htsmsg_get_u32(msg, "eventId", &u32))
  {
    tvherror("malformed eventDelete: 'eventId' missing");
    return;
  }
  tvhtrace("delete event %u", u32);
  
  /* Erase */
  SSchedules::iterator sit;
  for (sit = m_schedules.begin(); sit != m_schedules.end(); ++sit)
  {
    // Find the event so we can get the channel number
    SEvents::iterator eit = sit->second.events.find(u32);
    
    if (eit != sit->second.events.end())
    {
      tvhtrace("deleted event %d from channel %d", u32, sit->second.channel);
      sit->second.events.erase(eit);
      TriggerEpgUpdate(sit->second.channel);
      return;
    }
  }
}

uint32_t CTvheadend::GetNextUnnumberedChannelNumber()
{
  static uint32_t number = UNNUMBERED_CHANNEL;
  return number++;
}
