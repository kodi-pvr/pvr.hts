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
#include <ctime>
#include <memory>

#include "p8-platform/util/StringUtils.h"

#include "Tvheadend.h"
#include "tvheadend/utilities/Utilities.h"
#include "tvheadend/utilities/Logger.h"

using namespace std;
using namespace ADDON;
using namespace P8PLATFORM;
using namespace tvheadend;
using namespace tvheadend::entity;
using namespace tvheadend::utilities;

CTvheadend::CTvheadend()
  : m_streamchange(false), m_vfs(m_conn),
    m_queue((size_t)-1), m_asyncState(Settings::GetInstance().GetResponseTimeout()),
    m_timeRecordings(m_conn), m_autoRecordings(m_conn)
{
  for (int i = 0; i < 1 || i < Settings::GetInstance().GetTotalTuners(); i++)
  {
    m_dmx.push_back(new CHTSPDemuxer(m_conn));
  }
  m_dmx_active = m_dmx[0];
}

CTvheadend::~CTvheadend()
{
  for (auto *dmx : m_dmx)
  {
    delete dmx;
  }
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
  Logger::Log(LogLevel::LEVEL_ERROR, "malformed getDiskSpace response: 'totaldiskspace'/'freediskspace' missing");
  return PVR_ERROR_SERVER_ERROR;
}

std::string CTvheadend::GetImageURL ( const char *str )
{
  if (*str != '/')
  {
    if (strncmp(str, "imagecache/", 11) == 0)
      return m_conn.GetWebURL("/%s", str);

    return str;
  }
  else
  {
    return m_conn.GetWebURL("%s", str);
  }
}

void CTvheadend::QueryAvailableProfiles()
{
  /* Build message */
  htsmsg_t *m = htsmsg_create_map();

  /* Send */
  {
    CLockObject lock(m_conn.Mutex());
    m = m_conn.SendAndWait("getProfiles", m);
  }

  /* Validate */
  if (m == nullptr)
    return;

  htsmsg_t *l;
  htsmsg_field_t *f;

  if ((l = htsmsg_get_list(m, "profiles")) == NULL)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed getProfiles: 'profiles' missing");
    htsmsg_destroy(m);
    return;
  }

  /* Process */
  HTSMSG_FOREACH(f, l)
  {
    const char *str;
    Profile profile;

    if ((str = htsmsg_get_str(&f->hmf_msg, "uuid")) != NULL)
      profile.SetUuid(str);
    if ((str = htsmsg_get_str(&f->hmf_msg, "name")) != NULL)
      profile.SetName(str);
    if ((str = htsmsg_get_str(&f->hmf_msg, "comment")) != NULL)
      profile.SetComment(str);

    Logger::Log(LogLevel::LEVEL_DEBUG, "profile name: %s, comment: %s added",
             profile.GetName().c_str(), profile.GetComment().c_str());

    m_profiles.push_back(profile);
  }

  htsmsg_destroy(m);
}

bool CTvheadend::HasStreamingProfile(const std::string &streamingProfile) const
{
  return std::find_if(
      m_profiles.cbegin(),
      m_profiles.cend(),
      [&streamingProfile](const Profile &profile)
      {
        return profile.GetName() == streamingProfile;
      }
  ) != m_profiles.cend();
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

    for (const auto &entry : m_tags)
    {
      /* Does group contain channels of the requested type?             */
      /* Note: tvheadend groups can contain both radio and tv channels. */
      /*       Thus, one tvheadend group can 'map' to two Kodi groups.  */
      if (!entry.second.ContainsChannelType(bRadio))
        continue;

      PVR_CHANNEL_GROUP tag;
      memset(&tag, 0, sizeof(tag));

      strncpy(tag.strGroupName, entry.second.GetName().c_str(),
              sizeof(tag.strGroupName) - 1);
      tag.bIsRadio = bRadio;
      tag.iPosition = entry.second.GetIndex();
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

    // Find the tag
    const auto it = std::find_if(
      m_tags.cbegin(),
      m_tags.cend(),
      [group](const TagMapEntry &tag)
    {
      return tag.second.GetName() == group.strGroupName;
    });

    if (it != m_tags.cend())
    {
      // Find all channels in this group that are of the correct type
      for (const auto &channelId : it->second.GetChannels())
      {
        auto cit = m_channels.find(channelId);

        if (cit != m_channels.cend() && cit->second.IsRadio() == group.bIsRadio)
        {
          PVR_CHANNEL_GROUP_MEMBER gm;
          memset(&gm, 0, sizeof(PVR_CHANNEL_GROUP_MEMBER));
          strncpy(
            gm.strGroupName, group.strGroupName, sizeof(gm.strGroupName) - 1);
          gm.iChannelUniqueId = cit->second.GetId();
          gm.iChannelNumber = cit->second.GetNum();
          gms.push_back(gm);
        }
      }
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
    
    for (const auto &entry : m_channels)
    {
      const auto &channel = entry.second;

      if (radio != channel.IsRadio())
        continue;

      PVR_CHANNEL chn;
      memset(&chn, 0 , sizeof(PVR_CHANNEL));

      chn.iUniqueId         = channel.GetId();
      chn.bIsRadio          = channel.IsRadio();
      chn.iChannelNumber    = channel.GetNum();
      chn.iSubChannelNumber = channel.GetNumMinor();
      chn.iEncryptionSystem = channel.GetCaid();
      chn.bIsHidden         = false;
      strncpy(chn.strChannelName, channel.GetName().c_str(),
              sizeof(chn.strChannelName) - 1);
      strncpy(chn.strIconPath, channel.GetIcon().c_str(),
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
            std::max(30000, Settings::GetInstance().GetResponseTimeout()))) == NULL)
    return PVR_ERROR_SERVER_ERROR;

  /* Check for error */
  if (htsmsg_get_u32(m, "success", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed deleteDvrEntry/cancelDvrEntry response: 'success' missing");
    u32 = PVR_ERROR_FAILED;
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
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed updateDvrEntry response: 'success' missing");
    u32 = PVR_ERROR_FAILED;
  }
  htsmsg_destroy(m);

  return u32 > 0  ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
}

int CTvheadend::GetRecordingCount ( void )
{
  if (!m_asyncState.WaitForState(ASYNC_EPG))
    return 0;

  CLockObject lock(m_mutex);

  return std::count_if(
    m_recordings.cbegin(), 
    m_recordings.cend(), 
    [](const RecordingMapEntry &entry)
  {
    return entry.second.IsRecording();
  });
}

PVR_ERROR CTvheadend::GetRecordings ( ADDON_HANDLE handle )
{
  if (!m_asyncState.WaitForState(ASYNC_EPG))
    return PVR_ERROR_FAILED;
  
  std::vector<PVR_RECORDING> recs;
  {
    CLockObject lock(m_mutex);
    Channels::const_iterator cit;
    char buf[128];

    for (const auto &entry : m_recordings)
    {
      const auto &recording = entry.second;

      if (!recording.IsRecording())
        continue;

      /* Setup entry */
      PVR_RECORDING rec;
      memset(&rec, 0, sizeof(rec));

      /* Channel name and icon */
      if ((cit = m_channels.find(recording.GetChannel())) != m_channels.end())
      {
        strncpy(rec.strChannelName, cit->second.GetName().c_str(),
                sizeof(rec.strChannelName) - 1);

        strncpy(rec.strIconPath, cit->second.GetIcon().c_str(),
                sizeof(rec.strIconPath) - 1);
      }

      /* ID */
      snprintf(buf, sizeof(buf), "%i", recording.GetId());
      strncpy(rec.strRecordingId, buf, sizeof(rec.strRecordingId) - 1);

      /* Title */
      strncpy(rec.strTitle, recording.GetTitle().c_str(), sizeof(rec.strTitle) - 1);

      /* Subtitle */
      strncpy(rec.strEpisodeName, recording.GetSubtitle().c_str(), sizeof(rec.strEpisodeName) - 1);

      /* Description */
      strncpy(rec.strPlot, recording.GetDescription().c_str(), sizeof(rec.strPlot) - 1);

      /* Time/Duration */
      rec.recordingTime = (time_t)recording.GetStart();
      rec.iDuration = static_cast<int>(recording.GetStop() - recording.GetStart());

      /* Priority */
      rec.iPriority = recording.GetPriority();

      /* Lifetime (based on retention or removal) */
      rec.iLifetime = recording.GetLifetime();

      /* Directory */
      // TODO: Move this logic to GetPath(), alternatively GetMangledPath()
      if (recording.GetPath() != "")
      {
        size_t idx = recording.GetPath().rfind("/");
        if (idx == 0 || idx == string::npos)
          strncpy(rec.strDirectory, "/", sizeof(rec.strDirectory) - 1);
        else
        {
          std::string d = recording.GetPath().substr(0, idx);
          if (d[0] != '/')
            d = "/" + d;
          strncpy(rec.strDirectory, d.c_str(), sizeof(rec.strDirectory) - 1);
        }
      }

      /* EPG event id */
      rec.iEpgEventId = recording.GetEventId();

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
  htsmsg_t *list;
  htsmsg_field_t *f;
  int idx;
  
  /* Build request */
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", atoi(rec.strRecordingId));

  Logger::Log(LogLevel::LEVEL_DEBUG, "dvr get cutpoints id=%s", rec.strRecordingId);

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
      Logger::Log(LogLevel::LEVEL_ERROR, "malformed getDvrCutpoints response: invalid EDL entry, will ignore");
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

    Logger::Log(LogLevel::LEVEL_DEBUG, "edl start:%d end:%d action:%d", start, end, type);
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

namespace
{
struct TimerType : PVR_TIMER_TYPE
{
  TimerType(unsigned int id,
            unsigned int attributes,
            const std::string &description,
            const std::vector< std::pair<int, std::string> > &priorityValues
              = std::vector< std::pair<int, std::string> >(),
            const std::vector< std::pair<int, std::string> > &lifetimeValues
              = std::vector< std::pair<int, std::string> >(),
            const std::vector< std::pair<int, std::string> > &dupEpisodesValues
              = std::vector< std::pair<int, std::string> >())
  {
    memset(this, 0, sizeof(PVR_TIMER_TYPE));

    iId                              = id;
    iAttributes                      = attributes;
    iPrioritiesSize                  = priorityValues.size();
    iPrioritiesDefault               = Settings::GetInstance().GetDvrPriority();
    iPreventDuplicateEpisodesSize    = dupEpisodesValues.size();
    iPreventDuplicateEpisodesDefault = Settings::GetInstance().GetDvrDupdetect();
    iLifetimesSize                   = lifetimeValues.size();
    iLifetimesDefault                = Settings::GetInstance().GetDvrLifetime();

    strncpy(strDescription, description.c_str(), sizeof(strDescription) - 1);

    int i = 0;
    for (auto it = priorityValues.begin(); it != priorityValues.end(); ++it, ++i)
    {
      priorities[i].iValue = it->first;
      strncpy(priorities[i].strDescription, it->second.c_str(), sizeof(priorities[i].strDescription) - 1);
    }

    i = 0;
    for (auto it = dupEpisodesValues.begin(); it != dupEpisodesValues.end(); ++it, ++i)
    {
      preventDuplicateEpisodes[i].iValue = it->first;
      strncpy(preventDuplicateEpisodes[i].strDescription, it->second.c_str(), sizeof(preventDuplicateEpisodes[i].strDescription) - 1);
    }

    i = 0;
    for (auto it = lifetimeValues.begin(); it != lifetimeValues.end(); ++it, ++i)
    {
      lifetimes[i].iValue = it->first;
      strncpy(lifetimes[i].strDescription, it->second.c_str(), sizeof(lifetimes[i].strDescription) - 1);
    }
  }
};

} // unnamed namespace

PVR_ERROR CTvheadend::GetTimerTypes ( PVR_TIMER_TYPE types[], int *size )
{
  /* PVR_Timer.iPriority values and presentation.*/
  static std::vector< std::pair<int, std::string> > priorityValues;
  if (priorityValues.size() == 0)
  {
    priorityValues.push_back(std::make_pair(DVR_PRIO_UNIMPORTANT, XBMC->GetLocalizedString(30355)));
    priorityValues.push_back(std::make_pair(DVR_PRIO_LOW,         XBMC->GetLocalizedString(30354)));
    priorityValues.push_back(std::make_pair(DVR_PRIO_NORMAL,      XBMC->GetLocalizedString(30353)));
    priorityValues.push_back(std::make_pair(DVR_PRIO_HIGH,        XBMC->GetLocalizedString(30352)));
    priorityValues.push_back(std::make_pair(DVR_PRIO_IMPORTANT,   XBMC->GetLocalizedString(30351)));
  }

  /* PVR_Timer.iPreventDuplicateEpisodes values and presentation.*/
  static std::vector< std::pair<int, std::string> > deDupValues;
  if (deDupValues.size() == 0)
  {
    deDupValues.push_back(std::make_pair(DVR_AUTOREC_RECORD_ALL,                      XBMC->GetLocalizedString(30356)));
    deDupValues.push_back(std::make_pair(DVR_AUTOREC_RECORD_DIFFERENT_EPISODE_NUMBER, XBMC->GetLocalizedString(30357)));
    deDupValues.push_back(std::make_pair(DVR_AUTOREC_RECORD_DIFFERENT_SUBTITLE,       XBMC->GetLocalizedString(30358)));
    deDupValues.push_back(std::make_pair(DVR_AUTOREC_RECORD_DIFFERENT_DESCRIPTION,    XBMC->GetLocalizedString(30359)));
    deDupValues.push_back(std::make_pair(DVR_AUTOREC_RECORD_ONCE_PER_WEEK,            XBMC->GetLocalizedString(30360)));
    deDupValues.push_back(std::make_pair(DVR_AUTOREC_RECORD_ONCE_PER_DAY,             XBMC->GetLocalizedString(30361)));
  }

  /* PVR_Timer.iLifetime values and presentation.*/
  static std::vector< std::pair<int, std::string> > lifetimeValues;
  if (lifetimeValues.size() == 0)
  {
    lifetimeValues.push_back(std::make_pair(DVR_RET_1DAY,    XBMC->GetLocalizedString(30365)));
    lifetimeValues.push_back(std::make_pair(DVR_RET_3DAY,    StringUtils::Format(XBMC->GetLocalizedString(30366), 3)));
    lifetimeValues.push_back(std::make_pair(DVR_RET_5DAY,    StringUtils::Format(XBMC->GetLocalizedString(30366), 5)));
    lifetimeValues.push_back(std::make_pair(DVR_RET_1WEEK,   XBMC->GetLocalizedString(30367)));
    lifetimeValues.push_back(std::make_pair(DVR_RET_2WEEK,   StringUtils::Format(XBMC->GetLocalizedString(30368), 2)));
    lifetimeValues.push_back(std::make_pair(DVR_RET_3WEEK,   StringUtils::Format(XBMC->GetLocalizedString(30368), 3)));
    lifetimeValues.push_back(std::make_pair(DVR_RET_1MONTH,  XBMC->GetLocalizedString(30369)));
    lifetimeValues.push_back(std::make_pair(DVR_RET_2MONTH,  StringUtils::Format(XBMC->GetLocalizedString(30370), 2)));
    lifetimeValues.push_back(std::make_pair(DVR_RET_3MONTH,  StringUtils::Format(XBMC->GetLocalizedString(30370), 3)));
    lifetimeValues.push_back(std::make_pair(DVR_RET_6MONTH,  StringUtils::Format(XBMC->GetLocalizedString(30370), 6)));
    lifetimeValues.push_back(std::make_pair(DVR_RET_1YEAR,   XBMC->GetLocalizedString(30371)));
    lifetimeValues.push_back(std::make_pair(DVR_RET_2YEARS,  StringUtils::Format(XBMC->GetLocalizedString(30372), 2)));
    lifetimeValues.push_back(std::make_pair(DVR_RET_3YEARS,  StringUtils::Format(XBMC->GetLocalizedString(30372), 3)));
    if (m_conn.GetProtocol() >= 25)
      lifetimeValues.push_back(std::make_pair(DVR_RET_SPACE,   XBMC->GetLocalizedString(30373)));
    lifetimeValues.push_back(std::make_pair(DVR_RET_FOREVER, XBMC->GetLocalizedString(30374)));
  }

  unsigned int TIMER_ONCE_MANUAL_ATTRIBS
    = PVR_TIMER_TYPE_IS_MANUAL           |
      PVR_TIMER_TYPE_SUPPORTS_CHANNELS   |
      PVR_TIMER_TYPE_SUPPORTS_START_TIME |
      PVR_TIMER_TYPE_SUPPORTS_END_TIME   |
      PVR_TIMER_TYPE_SUPPORTS_PRIORITY   |
      PVR_TIMER_TYPE_SUPPORTS_LIFETIME;

  unsigned int TIMER_ONCE_EPG_ATTRIBS
    = PVR_TIMER_TYPE_SUPPORTS_CHANNELS          |
      PVR_TIMER_TYPE_SUPPORTS_START_TIME        |
      PVR_TIMER_TYPE_SUPPORTS_END_TIME          |
      PVR_TIMER_TYPE_REQUIRES_EPG_TAG_ON_CREATE |
      PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN  |
      PVR_TIMER_TYPE_SUPPORTS_PRIORITY          |
      PVR_TIMER_TYPE_SUPPORTS_LIFETIME;

  if (m_conn.GetProtocol() >= 23)
  {
    TIMER_ONCE_MANUAL_ATTRIBS |= PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE;
    TIMER_ONCE_EPG_ATTRIBS    |= PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE;
  }

  /* Timer types definition. */
  static std::vector< std::unique_ptr<TimerType> > timerTypes;
  if (timerTypes.size() == 0)
  {
    timerTypes.push_back(
      /* One-shot manual (time and channel based) */
      std::unique_ptr<TimerType>(new TimerType(
        /* Type id. */
        TIMER_ONCE_MANUAL,
        /* Attributes. */
        TIMER_ONCE_MANUAL_ATTRIBS,
        /* Let Kodi generate the description. */
        "",
        /* Values definitions for priorities. */
        priorityValues,
        /* Values definitions for lifetime. */
        lifetimeValues)));

    timerTypes.push_back(
      /* One-shot epg based */
      std::unique_ptr<TimerType>(new TimerType(
        /* Type id. */
        TIMER_ONCE_EPG,
        /* Attributes. */
        TIMER_ONCE_EPG_ATTRIBS,
        /* Let Kodi generate the description. */
        "",
        /* Values definitions for priorities. */
        priorityValues,
        /* Values definitions for lifetime. */
        lifetimeValues)));

    timerTypes.push_back(
      /* Read-only one-shot for timers generated by timerec */
      std::unique_ptr<TimerType>(new TimerType(
        /* Type id. */
        TIMER_ONCE_CREATED_BY_TIMEREC,
        /* Attributes. */
        TIMER_ONCE_MANUAL_ATTRIBS  |
        PVR_TIMER_TYPE_IS_READONLY |
        PVR_TIMER_TYPE_FORBIDS_NEW_INSTANCES,
        /* Description. */
        XBMC->GetLocalizedString(30350), // "One Time (Scheduled by repeating timer)"
        /* Values definitions for priorities. */
        priorityValues,
        /* Values definitions for lifetime. */
        lifetimeValues)));

    timerTypes.push_back(
      /* Read-only one-shot for timers generated by autorec */
      std::unique_ptr<TimerType>(new TimerType(
        /* Type id. */
        TIMER_ONCE_CREATED_BY_AUTOREC,
        /* Attributes. */
        TIMER_ONCE_EPG_ATTRIBS     |
        PVR_TIMER_TYPE_IS_READONLY |
        PVR_TIMER_TYPE_FORBIDS_NEW_INSTANCES,
        /* Description. */
        XBMC->GetLocalizedString(30350), // "One Time (Scheduled by repeating timer)"
        /* Values definitions for priorities. */
        priorityValues,
        /* Values definitions for lifetime. */
        lifetimeValues)));

    timerTypes.push_back(
      /* Repeating manual (time and channel based) - timerec */
      std::unique_ptr<TimerType>(new TimerType(
        /* Type id. */
        TIMER_REPEATING_MANUAL,
        /* Attributes. */
        PVR_TIMER_TYPE_IS_MANUAL                  |
        PVR_TIMER_TYPE_IS_REPEATING               |
        PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE    |
        PVR_TIMER_TYPE_SUPPORTS_CHANNELS          |
        PVR_TIMER_TYPE_SUPPORTS_START_TIME        |
        PVR_TIMER_TYPE_SUPPORTS_END_TIME          |
        PVR_TIMER_TYPE_SUPPORTS_WEEKDAYS          |
        PVR_TIMER_TYPE_SUPPORTS_PRIORITY          |
        PVR_TIMER_TYPE_SUPPORTS_LIFETIME          |
        PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS,
        /* Let Kodi generate the description. */
        "",
        /* Values definitions for priorities. */
        priorityValues,
        /* Values definitions for lifetime. */
        lifetimeValues)));

    unsigned int TIMER_REPEATING_EPG_ATTRIBS
      = PVR_TIMER_TYPE_IS_REPEATING                |
        PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE     |
        PVR_TIMER_TYPE_SUPPORTS_TITLE_EPG_MATCH    |
        PVR_TIMER_TYPE_SUPPORTS_CHANNELS           |
        PVR_TIMER_TYPE_SUPPORTS_START_TIME         |
        PVR_TIMER_TYPE_SUPPORTS_START_ANYTIME      |
        PVR_TIMER_TYPE_SUPPORTS_WEEKDAYS           |
        PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN   |
        PVR_TIMER_TYPE_SUPPORTS_PRIORITY           |
        PVR_TIMER_TYPE_SUPPORTS_LIFETIME           |
        PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS;

    if (m_conn.GetProtocol() >= 20)
    {
      TIMER_REPEATING_EPG_ATTRIBS |= PVR_TIMER_TYPE_SUPPORTS_FULLTEXT_EPG_MATCH;
      TIMER_REPEATING_EPG_ATTRIBS |= PVR_TIMER_TYPE_SUPPORTS_RECORD_ONLY_NEW_EPISODES;
    }

    if (!Settings::GetInstance().GetAutorecApproxTime())
    {
      /* We need the end time to represent the end of the tvh starting window */
      TIMER_REPEATING_EPG_ATTRIBS |= PVR_TIMER_TYPE_SUPPORTS_END_TIME;
      TIMER_REPEATING_EPG_ATTRIBS |= PVR_TIMER_TYPE_SUPPORTS_END_ANYTIME;
    }

    timerTypes.push_back(
      /* Repeating epg based - autorec */
      std::unique_ptr<TimerType>(new TimerType(
        /* Type id. */
        TIMER_REPEATING_EPG,
        /* Attributes. */
        TIMER_REPEATING_EPG_ATTRIBS,
        /* Let Kodi generate the description. */
        "",
        /* Values definitions for priorities. */
        priorityValues,
        /* Values definitions for lifetime. */
        lifetimeValues,
        /* Values definitions for prevent duplicate episodes. */
        deDupValues)));
  }

  /* Copy data to target array. */
  int i = 0;
  for (auto it = timerTypes.begin(); it != timerTypes.end(); ++it, ++i)
    types[i] = **it;

  *size = timerTypes.size();
  return PVR_ERROR_NO_ERROR;
}

int CTvheadend::GetTimerCount ( void )
{
  if (!m_asyncState.WaitForState(ASYNC_EPG))
    return 0;
  
  CLockObject lock(m_mutex);

  // Normal timers
  int timerCount = std::count_if(
    m_recordings.cbegin(),
    m_recordings.cend(),
    [](const RecordingMapEntry &entry)
  {
    return entry.second.IsTimer();
  });

  // Repeating timers
  timerCount += m_timeRecordings.GetTimerecTimerCount();
  timerCount += m_autoRecordings.GetAutorecTimerCount();

  return timerCount;
}

bool CTvheadend::CreateTimer ( const Recording &tvhTmr, PVR_TIMER &tmr )
{
  memset(&tmr, 0, sizeof(tmr));

  tmr.iClientIndex       = tvhTmr.GetId();
  tmr.iClientChannelUid  = (tvhTmr.GetChannel() > 0) ? tvhTmr.GetChannel() : -1;
  tmr.startTime          = static_cast<time_t>(tvhTmr.GetStart());
  tmr.endTime            = static_cast<time_t>(tvhTmr.GetStop());
  strncpy(tmr.strTitle,
          tvhTmr.GetTitle().c_str(), sizeof(tmr.strTitle) - 1);
  strncpy(tmr.strEpgSearchString,
          "", sizeof(tmr.strEpgSearchString) - 1); // n/a for one-shot timers
  strncpy(tmr.strDirectory,
          "", sizeof(tmr.strDirectory) - 1);       // n/a for one-shot timers
  strncpy(tmr.strSummary,
          tvhTmr.GetDescription().c_str(), sizeof(tmr.strSummary) - 1);

  if (m_conn.GetProtocol() >= 23)
    tmr.state            = !tvhTmr.IsEnabled()
                            ? PVR_TIMER_STATE_DISABLED
                            : tvhTmr.GetState();
  else
    tmr.state            = tvhTmr.GetState();

  tmr.iPriority          = tvhTmr.GetPriority();
  tmr.iLifetime          = tvhTmr.GetLifetime();
  tmr.iTimerType         = tvhTmr.GetTimerType();
  tmr.iMaxRecordings     = 0;                // not supported by tvh
  tmr.iRecordingGroup    = 0;                // not supported by tvh
  tmr.iPreventDuplicateEpisodes = 0;         // n/a for one-shot timers
  tmr.firstDay           = 0;                // not supported by tvh
  tmr.iWeekdays          = PVR_WEEKDAY_NONE; // n/a for one-shot timers
  tmr.iEpgUid            = (tvhTmr.GetEventId() > 0) ? tvhTmr.GetEventId() : -1;
  tmr.iMarginStart       = static_cast<unsigned int>(tvhTmr.GetStartExtra());
  tmr.iMarginEnd         = static_cast<unsigned int>(tvhTmr.GetStopExtra());
  tmr.iGenreType         = 0;                // not supported by tvh?
  tmr.iGenreSubType      = 0;                // not supported by tvh?
  tmr.bFullTextEpgSearch = false;            // n/a for one-shot timers
  tmr.iParentClientIndex = tmr.iTimerType == TIMER_ONCE_CREATED_BY_TIMEREC
                            ? m_timeRecordings.GetTimerIntIdFromStringId(tvhTmr.GetTimerecId())
                            : tmr.iTimerType == TIMER_ONCE_CREATED_BY_AUTOREC
                              ? m_autoRecordings.GetTimerIntIdFromStringId(tvhTmr.GetAutorecId())
                              : 0;
  return true;
}

PVR_ERROR CTvheadend::GetTimers ( ADDON_HANDLE handle )
{
  if (!m_asyncState.WaitForState(ASYNC_EPG))
    return PVR_ERROR_FAILED;
  
  std::vector<PVR_TIMER> timers;
  {
    CLockObject lock(m_mutex);

    /* One-shot timers */
    for (const auto &entry : m_recordings)
    {
      const auto &recording = entry.second;

      if (!recording.IsTimer())
        continue;

      /* Setup entry */
      PVR_TIMER tmr;
      if (CreateTimer(recording, tmr))
        timers.push_back(tmr);
    }

    /* Time-based repeating timers */
    m_timeRecordings.GetTimerecTimers(timers);

    /* EPG-query-based repeating timers */
    m_autoRecordings.GetAutorecTimers(timers);
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
  if ((timer.iTimerType == TIMER_ONCE_MANUAL) ||
      (timer.iTimerType == TIMER_ONCE_EPG))
  {
    /* one shot timer */

    uint32_t u32;

    /* Build message */
    htsmsg_t *m = htsmsg_create_map();
    if (timer.iEpgUid > PVR_TIMER_NO_EPG_UID && timer.iTimerType == TIMER_ONCE_EPG)
    {
      /* EPG-based timer */
      htsmsg_add_u32(m, "eventId",      timer.iEpgUid);
    }
    else
    {
      /* manual timer */
      htsmsg_add_str(m, "title",        timer.strTitle);

      int64_t start = timer.startTime;
      if (start == 0)
      {
        /* Instant timer. Adjust start time to 'now'. */
        start = time(NULL);
      }

      htsmsg_add_s64(m, "start",        start);
      htsmsg_add_s64(m, "stop",         timer.endTime);
      htsmsg_add_u32(m, "channelId",    timer.iClientChannelUid);
      htsmsg_add_str(m, "description",  timer.strSummary);
    }

    if (m_conn.GetProtocol() >= 23)
      htsmsg_add_u32(m, "enabled",  timer.state == PVR_TIMER_STATE_DISABLED ? 0 : 1);

    htsmsg_add_s64(m, "startExtra", timer.iMarginStart);
    htsmsg_add_s64(m, "stopExtra",  timer.iMarginEnd);

    if (m_conn.GetProtocol() >= 25)
    {
      htsmsg_add_u32(m, "removal",   timer.iLifetime);  // remove from disk
      htsmsg_add_u32(m, "retention", DVR_RET_ONREMOVE); // remove from tvh database
    }
    else
      htsmsg_add_u32(m, "retention", timer.iLifetime);  // remove from tvh database

    htsmsg_add_u32(m, "priority",   timer.iPriority);

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
      Logger::Log(LogLevel::LEVEL_ERROR, "malformed addDvrEntry response: 'success' missing");
      u32 = PVR_ERROR_FAILED;
    }
    htsmsg_destroy(m);

    return u32 > 0  ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
  }
  else if (timer.iTimerType == TIMER_REPEATING_MANUAL)
  {
    /* time-based repeating timer */
    return m_timeRecordings.SendTimerecAdd(timer);
  }
  else if (timer.iTimerType == TIMER_REPEATING_EPG)
  {
    /* EPG-query-based repeating timers */
    return m_autoRecordings.SendAutorecAdd(timer);
  }
  else
  {
    /* unknown timer */
    Logger::Log(LogLevel::LEVEL_ERROR, "unknown timer type");
    return PVR_ERROR_INVALID_PARAMETERS;
  }
}

PVR_ERROR CTvheadend::DeleteTimer
  ( const PVR_TIMER &timer, bool _unused(force) )
{
  if ((timer.iTimerType == TIMER_ONCE_MANUAL) ||
      (timer.iTimerType == TIMER_ONCE_EPG))
  {
    /* one shot timer */
    return SendDvrDelete(timer.iClientIndex, "cancelDvrEntry");
  }
  else if (timer.iTimerType == TIMER_REPEATING_MANUAL)
  {
    /* time-based repeating timer */
    return m_timeRecordings.SendTimerecDelete(timer);
  }
  else if (timer.iTimerType == TIMER_REPEATING_EPG)
  {
    /* EPG-query-based repeating timer */
    return m_autoRecordings.SendAutorecDelete(timer);
  }
  else if ((timer.iTimerType == TIMER_ONCE_CREATED_BY_TIMEREC) ||
           (timer.iTimerType == TIMER_ONCE_CREATED_BY_AUTOREC))
  {
    /* Read-only timer created by autorec or timerec */
    const auto &it = m_recordings.find(timer.iClientIndex);
    if (it != m_recordings.end() && it->second.IsRecording())
    {
      /* This is actually a request to cancel an active recording. */
      return SendDvrDelete(timer.iClientIndex, "cancelDvrEntry");
    }
    else
    {
      Logger::Log(LogLevel::LEVEL_ERROR, "timer is read-only");
      return PVR_ERROR_INVALID_PARAMETERS;
    }
  }
  else
  {
    /* unknown timer */
    Logger::Log(LogLevel::LEVEL_ERROR, "unknown timer type");
    return PVR_ERROR_INVALID_PARAMETERS;
  }
}

PVR_ERROR CTvheadend::UpdateTimer ( const PVR_TIMER &timer )
{
  if ((timer.iTimerType == TIMER_ONCE_MANUAL) ||
      (timer.iTimerType == TIMER_ONCE_EPG))
  {
    /* one shot timer */

    /* Build message */
    htsmsg_t *m = htsmsg_create_map();
    htsmsg_add_u32(m, "id",           timer.iClientIndex);

    if (m_conn.GetProtocol() >= 22)
    {
      /* support for updating the channel was added very late to the htsp protocol. */
      htsmsg_add_u32(m, "channelId", timer.iClientChannelUid);
    }
    else
    {
      const auto &it = m_recordings.find(timer.iClientIndex);
      if (it == m_recordings.end())
      {
        Logger::Log(LogLevel::LEVEL_ERROR, "cannot find the timer to update");
        return PVR_ERROR_INVALID_PARAMETERS;
      }

      if (it->second.GetChannel() != static_cast<uint32_t>(timer.iClientChannelUid))
      {
        Logger::Log(LogLevel::LEVEL_ERROR, "updating channels of one-shot timers not supported by HTSP v%d", m_conn.GetProtocol());
        return PVR_ERROR_NOT_IMPLEMENTED;
      }
    }

    htsmsg_add_str(m, "title",        timer.strTitle);

    if (m_conn.GetProtocol() >= 23)
      htsmsg_add_u32(m, "enabled",    timer.state == PVR_TIMER_STATE_DISABLED ? 0 : 1);

    int64_t start = timer.startTime;
    if (start == 0)
    {
      /* Instant timer. Adjust start time to 'now'. */
      start = time(NULL);
    }

    htsmsg_add_s64(m, "start",        start);
    htsmsg_add_s64(m, "stop",         timer.endTime);
    htsmsg_add_str(m, "description",  timer.strSummary);
    htsmsg_add_s64(m, "startExtra",   timer.iMarginStart);
    htsmsg_add_s64(m, "stopExtra",    timer.iMarginEnd);

    if (m_conn.GetProtocol() >= 25)
    {
      htsmsg_add_u32(m, "removal",    timer.iLifetime); // remove from disk
      htsmsg_add_u32(m, "retention",  DVR_RET_ONREMOVE);// remove from tvh database
    }
    else
      htsmsg_add_u32(m, "retention",  timer.iLifetime); // remove from tvh database

    htsmsg_add_u32(m, "priority",     timer.iPriority);

    return SendDvrUpdate(m);
  }
  else if (timer.iTimerType == TIMER_REPEATING_MANUAL)
  {
    /* time-based repeating timer */
    return m_timeRecordings.SendTimerecUpdate(timer);
  }
  else if (timer.iTimerType == TIMER_REPEATING_EPG)
  {
    /* EPG-query-based repeating timers */
    return m_autoRecordings.SendAutorecUpdate(timer);
  }
  else if ((timer.iTimerType == TIMER_ONCE_CREATED_BY_TIMEREC) ||
           (timer.iTimerType == TIMER_ONCE_CREATED_BY_AUTOREC))
  {
    if (m_conn.GetProtocol() >= 23)
    {
      /* Read-only timer created by autorec or timerec */
      const auto &it = m_recordings.find(timer.iClientIndex);
      if (it != m_recordings.end() &&
          (it->second.IsEnabled() == (timer.state == PVR_TIMER_STATE_DISABLED)))
      {
        /* This is actually a request to enable/disable a timer. */
        htsmsg_t *m = htsmsg_create_map();
        htsmsg_add_u32(m, "id",      timer.iClientIndex);
        htsmsg_add_u32(m, "enabled", timer.state == PVR_TIMER_STATE_DISABLED ? 0 : 1);
        return SendDvrUpdate(m);
      }
    }

    Logger::Log(LogLevel::LEVEL_ERROR, "timer is read-only");
    return PVR_ERROR_INVALID_PARAMETERS;
  }
  else
  {
    /* unknown timer */
    Logger::Log(LogLevel::LEVEL_ERROR, "unknown timer type");
    return PVR_ERROR_INVALID_PARAMETERS;
  }
}

/* **************************************************************************
 * EPG
 * *************************************************************************/

/* Transfer schedule to XBMC */
void CTvheadend::TransferEvent
  ( ADDON_HANDLE handle, const Event &event )
{
  /* Build */
  EPG_TAG epg;
  memset(&epg, 0, sizeof(EPG_TAG));
  epg.iUniqueBroadcastId  = event.GetId();
  epg.strTitle            = event.GetTitle().c_str();
  epg.iChannelNumber      = event.GetChannel();
  epg.startTime           = event.GetStart();
  epg.endTime             = event.GetStop();
  epg.strPlotOutline      = event.GetSummary().c_str();
  epg.strPlot             = event.GetDesc().c_str();
  epg.strOriginalTitle    = NULL; /* not supported by tvh */
  epg.strCast             = NULL; /* not supported by tvh */
  epg.strDirector         = NULL; /* not supported by tvh */
  epg.strWriter           = NULL; /* not supported by tvh */
  epg.iYear               = 0;    /* not supported by tvh */
  epg.strIMDBNumber       = NULL; /* not supported by tvh */
  epg.strIconPath         = event.GetImage().c_str();
  epg.iGenreType          = event.GetContent() & 0xF0;
  epg.iGenreSubType       = event.GetContent() & 0x0F;
  epg.strGenreDescription = NULL; /* not supported by tvh */
  epg.firstAired          = event.GetAired();
  epg.iParentalRating     = event.GetAge();
  epg.iStarRating         = event.GetStars();
  epg.bNotify             = false; /* not supported by tvh */
  epg.iSeriesNumber       = event.GetSeason();
  epg.iEpisodeNumber      = event.GetEpisode();
  epg.iEpisodePartNumber  = event.GetPart();
  epg.strEpisodeName      = event.GetSubtitle().c_str();
  epg.iFlags              = EPG_TAG_FLAG_UNDEFINED;

  /* Callback. */
  PVR->TransferEpgEntry(handle, &epg);
}

PVR_ERROR CTvheadend::GetEpg
  ( ADDON_HANDLE handle, const PVR_CHANNEL &chn, time_t start, time_t end )
{
  htsmsg_field_t *f;
  int n = 0;

  Logger::Log(LogLevel::LEVEL_TRACE, "get epg channel %d start %ld stop %ld", chn.iUniqueId,
           (long long)start, (long long)end);

  /* Async transfer */
  if (Settings::GetInstance().GetAsyncEpg())
  {
    if (!m_asyncState.WaitForState(ASYNC_DONE))
      return PVR_ERROR_FAILED;
    
    // Find the relevant events
    Segment segment;
    {
      CLockObject lock(m_mutex);
      auto sit = m_schedules.find(chn.iUniqueId);

      if (sit != m_schedules.cend())
        segment = sit->second.GetSegment(start, end);
    }

    // Transfer
    for (const auto &event : segment)
      TransferEvent(handle, event);

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
      Logger::Log(LogLevel::LEVEL_ERROR, "malformed getEvents response: 'events' missing");
      return PVR_ERROR_SERVER_ERROR;
    }
    HTSMSG_FOREACH(f, l)
    {
      Event event;
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

  Logger::Log(LogLevel::LEVEL_TRACE, "get epg channel %d events %d", chn.iUniqueId, n);

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

  /* Rebuild state */
  for (auto *dmx : m_dmx)
  {
    dmx->Connected();
  }
  m_vfs.Connected();
  m_timeRecordings.Connected();
  m_autoRecordings.Connected();

  /* Flag all async fields in case they've been deleted */
  for (auto &entry : m_channels)
    entry.second.SetDirty(true);
  for (auto &entry : m_tags)
    entry.second.SetDirty(true);
  for (auto &entry : m_recordings)
    entry.second.SetDirty(true);
  for (auto &entry : m_schedules)
    entry.second.SetDirty(true);

  /* Request Async data */
  m_asyncState.SetState(ASYNC_NONE);
  
  msg = htsmsg_create_map();
  htsmsg_add_u32(msg, "epg", Settings::GetInstance().GetAsyncEpg());
  //htsmsg_add_u32(msg, "epgMaxTime", 0);
  //htsmsg_add_s64(msg, "lastUpdate", 0);
  if ((msg = m_conn.SendAndWait0("enableAsyncMetadata", msg)) == NULL)
    return false;

  htsmsg_destroy(msg);
  Logger::Log(LogLevel::LEVEL_DEBUG, "async updates requested");

  return true;
}

/* **************************************************************************
 * Message handling
 * *************************************************************************/

bool CTvheadend::ProcessMessage ( const char *method, htsmsg_t *msg )
{
  uint32_t subId;

  if (!htsmsg_get_u32(msg, "subscriptionId", &subId))
  {
    /* subscriptionId found - for a Demuxer */
    for (auto *dmx : m_dmx)
    {
      if (dmx->GetSubscriptionId() == subId)
        return dmx->ProcessMessage(method, msg);
    }
    return true;
  }

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

    SHTSPEventList eventsCopy;
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

      /* Timerec */
      else if (!strcmp("timerecEntryAdd", method))
      {
        if (m_timeRecordings.ParseTimerecAddOrUpdate(msg.m_msg, true))
          TriggerTimerUpdate();
      }
      else if (!strcmp("timerecEntryUpdate", method))
      {
        if (m_timeRecordings.ParseTimerecAddOrUpdate(msg.m_msg, false))
          TriggerTimerUpdate();
      }
      else if (!strcmp("timerecEntryDelete", method))
      {
        if (m_timeRecordings.ParseTimerecDelete(msg.m_msg))
          TriggerTimerUpdate();
      }

      /* Autorec */
      else if (!strcmp("autorecEntryAdd", method))
      {
        if (m_autoRecordings.ParseAutorecAddOrUpdate(msg.m_msg, true))
          TriggerTimerUpdate();
      }
      else if (!strcmp("autorecEntryUpdate", method))
      {
        if (m_autoRecordings.ParseAutorecAddOrUpdate(msg.m_msg, false))
          TriggerTimerUpdate();
      }
      else if (!strcmp("autorecEntryDelete", method))
      {
        if (m_autoRecordings.ParseAutorecDelete(msg.m_msg))
          TriggerTimerUpdate();
      }

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
        Logger::Log(LogLevel::LEVEL_DEBUG, "unhandled message [%s]", method);

      /* make a copy of events list to process it without lock. */
      eventsCopy = m_events;
      m_events.clear();
    }
  
    /* Manual delete rather than waiting */
    htsmsg_destroy(msg.m_msg);
    msg.m_msg = NULL;

    /* Process events
     * Note: due to potential deadly embrace this must be done without the
     *       m_mutex held!
     */
    SHTSPEventList::const_iterator it;
    for (it = eventsCopy.begin(); it != eventsCopy.end(); ++it)
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

  /* Query the server for available streaming profiles */
  QueryAvailableProfiles();

  /* Show a notification if the profile is not available */
  std::string streamingProfile = Settings::GetInstance().GetStreamingProfile();

  if (!streamingProfile.empty() && !HasStreamingProfile(streamingProfile))
  {
    XBMC->QueueNotification(
        QUEUE_ERROR,
        XBMC->GetLocalizedString(30502), streamingProfile.c_str());
  }
  else
  {
    /* Tell each demuxer to use this profile from now on */
    for (auto *dmx : m_dmx)
      dmx->SetStreamingProfile(streamingProfile);
  }
}

void CTvheadend::SyncChannelsCompleted ( void )
{
  /* Already done */
  if (m_asyncState.GetState() > ASYNC_CHN)
    return;

  /* Tags */
  utilities::erase_if(m_tags, [](const TagMapEntry &entry)
  {
    return entry.second.IsDirty();
  });

  TriggerChannelGroupsUpdate();

  /* Channels */
  utilities::erase_if(m_channels, [](const ChannelMapEntry &entry)
  {
    return entry.second.IsDirty();
  });

  TriggerChannelUpdate();
  
  /* Next */
  m_asyncState.SetState(ASYNC_DVR);
}

void CTvheadend::SyncDvrCompleted ( void )
{
  /* Done */
  if (m_asyncState.GetState() > ASYNC_DVR)
    return;

  /* Recordings */
  utilities::erase_if(m_recordings, [](const RecordingMapEntry &entry)
  {
    return entry.second.IsDirty();
  });

  /* Time-based repeating timers */
  m_timeRecordings.SyncDvrCompleted();

  /* EPG-query-based repeating timers */
  m_autoRecordings.SyncDvrCompleted();

  TriggerRecordingUpdate();
  TriggerTimerUpdate();

  /* Next */
  m_asyncState.SetState(ASYNC_EPG);
}

void CTvheadend::SyncEpgCompleted ( void )
{
  /* Done */
  if (!Settings::GetInstance().GetAsyncEpg() || m_asyncState.GetState() > ASYNC_EPG)
    return;

  /* Schedules */
  utilities::erase_if(m_schedules, [](const ScheduleMapEntry &entry)
  {
    return entry.second.IsDirty();
  });

  /* Events */
  for (auto &entry : m_schedules)
  {
    utilities::erase_if(entry.second.GetEvents(), [](const EventMapEntry &entry)
    {
      return entry.second.IsDirty();
    });
  }
  
  /* Trigger updates */
  for (const auto &entry : m_schedules)
    TriggerEpgUpdate(entry.second.GetId());
}

void CTvheadend::ParseTagAddOrUpdate ( htsmsg_t *msg, bool bAdd )
{
  uint32_t u32;
  const char *str;
  htsmsg_t *list;

  /* Validate */
  if (htsmsg_get_u32(msg, "tagId", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed tagAdd/tagUpdate: 'tagId' missing");
    return;
  }

  /* Locate object */
  auto &existingTag = m_tags[u32];
  existingTag.SetDirty(false);
  
  /* Create new object */
  Tag tag;
  tag.SetId(u32);

  /* Index */
  if (!htsmsg_get_u32(msg, "tagIndex", &u32))
    tag.SetIndex(u32);

  /* Name */
  if ((str = htsmsg_get_str(msg, "tagName")) != NULL)
    tag.SetName(str);
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed tagAdd: 'tagName' missing");
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

    Logger::Log(LogLevel::LEVEL_DEBUG, "tag updated id:%u, name:%s",
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
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed tagDelete: 'tagId' missing");
    return;
  }
  Logger::Log(LogLevel::LEVEL_DEBUG, "delete tag %u", u32);
  
  /* Erase */
  m_tags.erase(u32);
  TriggerChannelGroupsUpdate();
}

void CTvheadend::ParseChannelAddOrUpdate ( htsmsg_t *msg, bool bAdd )
{
  uint32_t u32;
  const char *str;
  htsmsg_t *list;

  /* Validate */
  if (htsmsg_get_u32(msg, "channelId", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed channelAdd/channelUpdate: 'channelId' missing");
    return;
  }

  /* Locate channel object */
  Channel &channel = m_channels[u32];
  Channel comparison = channel;
  channel.SetId(u32);
  channel.SetDirty(false);

  /* Channel name */
  if ((str = htsmsg_get_str(msg, "channelName")) != NULL)
    channel.SetName(str);
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed channelAdd: 'channelName' missing");
    return;
  }

  /* Channel number */
  if (!htsmsg_get_u32(msg, "channelNumber", &u32))
  {
    if (!u32)
      u32 = GetNextUnnumberedChannelNumber();
    channel.SetNum(u32);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed channelAdd: 'channelNumber' missing");
    return;
  }
  else if (!channel.GetNum())
    channel.SetNum(GetNextUnnumberedChannelNumber());
  
  /* ATSC subchannel number */
  if (!htsmsg_get_u32(msg, "channelNumberMinor", &u32))
    channel.SetNumMinor(u32);

  /* Channel icon */
  if ((str = htsmsg_get_str(msg, "channelIcon")) != NULL)
    channel.SetIcon(GetImageURL(str));

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

    channel.SetRadio(radio);
    channel.SetCaid(caid);
  }

  /* Update Kodi */
  if (channel != comparison)
  {
    Logger::Log(LogLevel::LEVEL_DEBUG, "channel %s id:%u, name:%s",
             (bAdd ? "added" : "updated"), channel.GetId(), channel.GetName().c_str());

    if (bAdd)
      m_channelTuningPredictor.AddChannel(channel);
    else
      m_channelTuningPredictor.UpdateChannel(comparison, channel);

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
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed channelDelete: 'channelId' missing");
    return;
  }
  Logger::Log(LogLevel::LEVEL_DEBUG, "delete channel %u", u32);
  
  /* Erase */
  m_channels.erase(u32);
  m_channelTuningPredictor.RemoveChannel(u32);
  TriggerChannelUpdate();
}

void CTvheadend::ParseRecordingAddOrUpdate ( htsmsg_t *msg, bool bAdd )
{
  const char *state, *str;
  uint32_t id, channel, eventId, retention, removal, priority, enabled;
  int64_t start, stop, startExtra, stopExtra;

  /* Channels must be complete */
  SyncChannelsCompleted();

  /* Validate */
  if (htsmsg_get_u32(msg, "id", &id))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd/dvrEntryUpdate: 'id' missing");
    return;
  }

  if (htsmsg_get_s64(msg, "start", &start) && bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd: 'start' missing");
    return;
  }

  if (htsmsg_get_s64(msg, "stop", &stop) && bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd: 'stop' missing");
    return;
  }

  if (((state = htsmsg_get_str(msg, "state")) == NULL) && bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd: 'state' missing");
    return;
  }

  /* Get entry */
  Recording &rec = m_recordings[id];
  Recording comparison = rec;
  rec.SetId(id);
  rec.SetDirty(false);
  rec.SetStart(start);
  rec.SetStop(stop);

  /* Channel is optional, it may not exist anymore */
  if (!htsmsg_get_u32(msg, "channel", &channel))
    rec.SetChannel(channel);

  if (!htsmsg_get_s64(msg, "startExtra", &startExtra))
    rec.SetStartExtra(startExtra);
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd: 'startExtra' missing");
    return;
  }

  if (!htsmsg_get_s64(msg, "stopExtra", &stopExtra))
    rec.SetStopExtra(stopExtra);
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd: 'stopExtra' missing");
    return;
  }

  if (m_conn.GetProtocol() >= 25)
  {
    if (!htsmsg_get_u32(msg, "removal", &removal))
      rec.SetLifetime(removal);
    else if (bAdd)
    {
      Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd: 'removal' missing");
      return;
    }
  }
  else
  {
    if (!htsmsg_get_u32(msg, "retention", &retention))
      rec.SetLifetime(retention);
    else if (bAdd)
    {
      Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd: 'retention' missing");
      return;
    }
  }

  if (!htsmsg_get_u32(msg, "priority", &priority))
  {
    switch (priority)
    {
      case DVR_PRIO_IMPORTANT:
      case DVR_PRIO_HIGH:
      case DVR_PRIO_NORMAL:
      case DVR_PRIO_LOW:
      case DVR_PRIO_UNIMPORTANT:
        rec.SetPriority(priority);
        break;
      default:
        Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd/dvrEntryUpdate: unknown priority value %d", priority);
        return;
    }
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd: 'priority' missing");
    return;
  }

  if (state != NULL)
  {
    /* Parse state */
    if      (strstr(state, "scheduled") != NULL)
      rec.SetState(PVR_TIMER_STATE_SCHEDULED);
    else if (strstr(state, "recording") != NULL)
      rec.SetState(PVR_TIMER_STATE_RECORDING);
    else if (strstr(state, "completed") != NULL)
      rec.SetState(PVR_TIMER_STATE_COMPLETED);
    else if (strstr(state, "missed") != NULL)
      rec.SetState(PVR_TIMER_STATE_ERROR);
    else if (strstr(state, "invalid") != NULL)
      rec.SetState(PVR_TIMER_STATE_ERROR);
  }

  /* Add optional fields */
  if (!htsmsg_get_u32(msg, "eventId", &eventId))
    rec.SetEventId(eventId);
  if (!htsmsg_get_u32(msg, "enabled", &enabled))
    rec.SetEnabled(enabled);
  if ((str = htsmsg_get_str(msg, "title")) != NULL)
    rec.SetTitle(str);
  if ((str = htsmsg_get_str(msg, "subtitle")) != NULL)
    rec.SetSubtitle(str);
  if ((str = htsmsg_get_str(msg, "path")) != NULL)
    rec.SetPath(str);
  if ((str = htsmsg_get_str(msg, "description")) != NULL)
    rec.SetDescription(str);
  // TODO: What?
  else if ((str = htsmsg_get_str(msg, "summary")) != NULL)
    rec.SetDescription(str);
  if ((str = htsmsg_get_str(msg, "timerecId")) != NULL)
    rec.SetTimerecId(str);
  if ((str = htsmsg_get_str(msg, "autorecId")) != NULL)
    rec.SetAutorecId(str);

  /* Error */
  if ((str = htsmsg_get_str(msg, "error")) != NULL)
  {
    if (!strcmp(str, "300"))
      rec.SetState(PVR_TIMER_STATE_ABORTED);
    else if (strstr(str, "missing") != NULL)
      rec.SetState(PVR_TIMER_STATE_ERROR);
    else
      rec.SetError(str);
  }
  
  /* A running recording will have an active subscription assigned to it */
  if (rec.GetState() == PVR_TIMER_STATE_RECORDING)
  {
    /* Parse subscription error */
    /* This field is absent when everything is fine or when htsp version < 20 */
    if ((str = htsmsg_get_str(msg, "subscriptionError")) != NULL)
    {
      /* No free adapter, AKA subscription conflict */
      if (!strcmp("noFreeAdapter", str))
        rec.SetState(PVR_TIMER_STATE_CONFLICT_NOK);
    }
  }

  /* Update */
  if (rec != comparison)
  {
    std::string error = rec.GetError().empty() ? "none" : rec.GetError();

    Logger::Log(LogLevel::LEVEL_DEBUG, "recording id:%d, state:%s, title:%s, desc:%s, error:%s",
             rec.GetId(), state, rec.GetTitle().c_str(), rec.GetDescription().c_str(),
             error.c_str());

    if (m_asyncState.GetState() > ASYNC_DVR)
    {
      TriggerTimerUpdate();
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
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryDelete: 'id' missing");
    return;
  }
  Logger::Log(LogLevel::LEVEL_DEBUG, "delete recording %u", u32);
  
  /* Erase */
  m_recordings.erase(u32);

  /* Update */
  TriggerTimerUpdate();
  TriggerRecordingUpdate();
}

bool CTvheadend::ParseEvent ( htsmsg_t *msg, bool bAdd, Event &evt )
{
  const char *str;
  uint32_t u32, id, channel;
  int64_t s64, start, stop;

  /* Recordings complete */
  SyncDvrCompleted();

  /* Validate */
  if (htsmsg_get_u32(msg, "eventId", &id))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed eventAdd/eventUpdate: 'eventId' missing");
    return false;
  }

  if (htsmsg_get_u32(msg, "channelId", &channel) && bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed eventAdd: 'channelId' missing");
    return false;
  }

  if (htsmsg_get_s64(msg, "start", &start) && bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed eventAdd: 'start' missing");
    return false;
  }

  if (htsmsg_get_s64(msg, "stop", &stop) && bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed eventAdd: 'stop' missing");
    return false;
  }

  evt.SetId(id);
  evt.SetChannel(channel);
  evt.SetStart((time_t)start);
  evt.SetStop((time_t)stop);

  /* Add optional fields */
  if ((str = htsmsg_get_str(msg, "title")) != NULL)
    evt.SetTitle(str);
  if ((str = htsmsg_get_str(msg, "subtitle")) != NULL)
    evt.SetSubtitle(str);
  if ((str = htsmsg_get_str(msg, "summary")) != NULL)
    evt.SetSummary(str);
  if ((str = htsmsg_get_str(msg, "description")) != NULL)
    evt.SetDesc(str);
  if ((str = htsmsg_get_str(msg, "image")) != NULL)
    evt.SetImage(str);
  if (!htsmsg_get_u32(msg, "nextEventId", &u32))
    evt.SetNext(u32);
  if (!htsmsg_get_u32(msg, "contentType", &u32))
    evt.SetContent(u32);
  if (!htsmsg_get_u32(msg, "starRating", &u32))
    evt.SetStars(u32);
  if (!htsmsg_get_u32(msg, "ageRating", &u32))
    evt.SetAge(u32);
  if (!htsmsg_get_s64(msg, "firstAired", &s64))
    evt.SetAired((time_t)s64);
  if (!htsmsg_get_u32(msg, "seasonNumber", &u32))
    evt.SetSeason(u32);
  if (!htsmsg_get_u32(msg, "episodeNumber", &u32))
    evt.SetEpisode(u32);
  if (!htsmsg_get_u32(msg, "partNumber", &u32))
    evt.SetPart(u32);

  /* Add optional recording link */
  auto rit = std::find_if(
    m_recordings.cbegin(), 
    m_recordings.cend(), 
    [evt](const RecordingMapEntry &entry)
  {
    return entry.second.GetEventId() == evt.GetId();
  });

  if (rit != m_recordings.cend())
    evt.SetRecordingId(evt.GetId());
  
  return true;
}

void CTvheadend::ParseEventAddOrUpdate ( htsmsg_t *msg, bool bAdd )
{
  Event tmp;

  /* Parse */
  if (!ParseEvent(msg, bAdd, tmp))
    return;

  /* Get event handle */
  Schedule &sched  = m_schedules[tmp.GetChannel()];
  Events   &events = sched.GetEvents();
  Event    &evt    = events[tmp.GetId()];
  Event comparison = evt;
  sched.SetId(tmp.GetChannel());
  sched.SetDirty(false);
  evt.SetId(tmp.GetId());
  evt.SetDirty(false);
  
  /* Store */
  evt = tmp;

  /* Update */
  if (evt != comparison)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "event id:%d channel:%d start:%d stop:%d title:%s desc:%s",
             evt.GetId(), evt.GetChannel(), (int)evt.GetStart(), (int)evt.GetStop(),
             evt.GetTitle().c_str(), evt.GetDesc().c_str());

    if (m_asyncState.GetState() > ASYNC_EPG)
      TriggerEpgUpdate(tmp.GetChannel());
  }
}

void CTvheadend::ParseEventDelete ( htsmsg_t *msg )
{
  uint32_t u32;
  
  /* Validate */
  if (htsmsg_get_u32(msg, "eventId", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed eventDelete: 'eventId' missing");
    return;
  }
  Logger::Log(LogLevel::LEVEL_TRACE, "delete event %u", u32);
  
  /* Erase */
  for (auto &entry : m_schedules)
  {
    Schedule &schedule = entry.second;
    Events &events = schedule.GetEvents();

    // Find the event so we can get the channel number
    auto eit = events.find(u32);

    if (eit != events.end())
    {
      Logger::Log(LogLevel::LEVEL_TRACE, "deleted event %d from channel %d", u32, schedule.GetId());
      events.erase(eit);
      TriggerEpgUpdate(schedule.GetId());
      return;
    }
  }
}

uint32_t CTvheadend::GetNextUnnumberedChannelNumber()
{
  static uint32_t number = UNNUMBERED_CHANNEL;
  return number++;
}

void CTvheadend::TuneOnOldest( uint32_t channelId )
{
  CHTSPDemuxer* oldest = NULL;

  for (auto *dmx : m_dmx)
  {
    if (dmx->GetChannelId() == channelId)
    {
      dmx->Weight(SUBSCRIPTION_WEIGHT_PRETUNING);
      return;
    }
    if (dmx == m_dmx_active)
      continue;
    if (oldest == NULL || dmx->GetLastUse() <= oldest->GetLastUse())
      oldest = dmx;
  }
  if (oldest)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "pretuning channel %u on subscription %u",
             m_channels[channelId].GetNum(), oldest->GetSubscriptionId());
    oldest->Open(channelId, SUBSCRIPTION_WEIGHT_PRETUNING);
  }
}

void CTvheadend::PredictiveTune( uint32_t fromChannelId, uint32_t toChannelId )
{
  CLockObject lock(m_mutex);

  /* Consult the predictive tuning helper for which channel
   * should be predictably tuned next */
  uint32_t predictedChannelId = m_channelTuningPredictor.PredictNextChannelId(fromChannelId, toChannelId);

  if (predictedChannelId != predictivetune::CHANNEL_ID_NONE)
    TuneOnOldest(predictedChannelId);
}

bool CTvheadend::DemuxOpen( const PVR_CHANNEL &chn )
{
  CHTSPDemuxer *oldest;
  uint32_t prevId;
  bool ret;

  oldest = m_dmx[0];

  if (m_dmx.size() == 1)
  {
    /* speedup things if we don't use predictive tuning */
    ret = oldest->Open(chn.iUniqueId, SUBSCRIPTION_WEIGHT_SERVERCONF);
    m_dmx_active = oldest;
    return ret;
  }

  /* If we have a lingering subscription for the target channel
   * we reuse that subscription */
  for (auto *dmx : m_dmx)
  {
    if (dmx != m_dmx_active && dmx->GetChannelId() == chn.iUniqueId)
    {
      Logger::Log(LogLevel::LEVEL_TRACE, "retuning channel %u on subscription %u",
               m_channels[chn.iUniqueId].GetNum(), dmx->GetSubscriptionId());

      /* Lower the priority on the current subscrption */
      m_dmx_active->Weight(SUBSCRIPTION_WEIGHT_POSTTUNING);
      prevId = m_dmx_active->GetChannelId();

      /* Promote the lingering subscription to the active one */
      dmx->Weight(SUBSCRIPTION_WEIGHT_NORMAL);
      m_dmx_active = dmx;

      PredictiveTune(prevId, chn.iUniqueId);
      m_streamchange = true;
      return true;
    }
    if (dmx->GetLastUse() < oldest->GetLastUse())
      oldest = dmx;
  }

  /* If we don't have an existing subscription for the channel we create one
   * on the oldest demuxer */
  Logger::Log(LogLevel::LEVEL_TRACE, "tuning channel %u on subscription %u",
           m_channels[chn.iUniqueId].GetNum(), oldest->GetSubscriptionId());

  prevId = m_dmx_active->GetChannelId();
  m_dmx_active->Weight(SUBSCRIPTION_WEIGHT_POSTTUNING);

  ret = oldest->Open(chn.iUniqueId, SUBSCRIPTION_WEIGHT_NORMAL);
  m_dmx_active = oldest;
  if (ret)
    PredictiveTune(prevId, chn.iUniqueId);
  return ret;
}

DemuxPacket* CTvheadend::DemuxRead ( void )
{
  DemuxPacket *pkt = NULL;

  if (m_streamchange)
  {
    /* when switching to a previously used channel, we have to trigger a stream
     * change update through kodi. We don't queue that through the dmx packet
     * buffer, as we really want to use the currently queued packets for
     * immediate playback. */
    pkt = PVR->AllocateDemuxPacket(0);
    pkt->iStreamId = DMX_SPECIALID_STREAMCHANGE;
    m_streamchange = false;
    return pkt;
  }

  for (auto *dmx : m_dmx)
  {
    if (dmx == m_dmx_active)
      pkt = dmx->Read();
    else
    {
      /* Close "expired" subscriptions */
      if (dmx->GetChannelId() && Settings::GetInstance().GetPreTunerCloseDelay() &&
          dmx->GetLastUse() + Settings::GetInstance().GetPreTunerCloseDelay() < time(NULL))
      {
        Logger::Log(LogLevel::LEVEL_TRACE, "untuning channel %u on subscription %u",
                 m_channels[dmx->GetChannelId()].GetNum(), dmx->GetSubscriptionId());
        dmx->Close();
      }
      else
        dmx->Trim();
    }
  }
  return pkt;
}

void CTvheadend::DemuxClose ( void )
{
  for (auto *dmx : m_dmx)
  {
    dmx->Close();
  }
}

void CTvheadend::DemuxFlush ( void )
{
  for (auto *dmx : m_dmx)
  {
    dmx->Flush();
  }
}

void CTvheadend::DemuxAbort ( void )
{
  for (auto *dmx : m_dmx)
  {
    dmx->Abort();
  }
}

bool CTvheadend::DemuxSeek ( int time, bool backward, double *startpts )
{
  return m_dmx_active->Seek(time, backward, startpts);
}

void CTvheadend::DemuxSpeed ( int speed )
{
  m_dmx_active->Speed(speed);
}

PVR_ERROR CTvheadend::DemuxCurrentStreams ( PVR_STREAM_PROPERTIES *streams )
{
  return m_dmx_active->CurrentStreams(streams);
}

PVR_ERROR CTvheadend::DemuxCurrentSignal ( PVR_SIGNAL_STATUS &sig )
{
  return m_dmx_active->CurrentSignal(sig);
}

int64_t CTvheadend::DemuxGetTimeshiftTime() const
{
  return m_dmx_active->GetTimeshiftTime();
}

int64_t CTvheadend::DemuxGetTimeshiftBufferStart() const
{
  return m_dmx_active->GetTimeshiftBufferStart();
}

int64_t CTvheadend::DemuxGetTimeshiftBufferEnd() const
{
  return m_dmx_active->GetTimeshiftBufferEnd();
}

bool CTvheadend::DemuxIsRealTimeStream() const
{
  return m_dmx_active->IsRealTimeStream();
}

