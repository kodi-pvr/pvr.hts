/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Tvheadend.h"

#include "tvheadend/HTSPConnection.h"
#include "tvheadend/HTSPDemuxer.h"
#include "tvheadend/HTSPMessage.h"
#include "tvheadend/HTSPVFS.h"
#include "tvheadend/InstanceSettings.h"
#include "tvheadend/utilities/LifetimeMapper.h"
#include "tvheadend/utilities/Logger.h"
#include "tvheadend/utilities/Utilities.h"

#include "kodi/General.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>

using namespace tvheadend;
using namespace tvheadend::entity;
using namespace tvheadend::utilities;

CTvheadend::CTvheadend(const kodi::addon::IInstanceInfo& instance)
  : kodi::addon::CInstancePVRClient(instance),
    m_settings(new InstanceSettings(*this)),
    m_conn(new HTSPConnection(m_settings, *this)),
    m_streamchange(false),
    m_vfs(new HTSPVFS(m_settings, *m_conn)),
    m_queue(static_cast<size_t>(-1)),
    m_asyncState(m_settings->GetResponseTimeout()),
    m_timeRecordings(*m_conn),
    m_autoRecordings(m_settings, *m_conn),
    m_epgMaxDays(EpgMaxFutureDays()),
    m_playingLiveStream(false),
    m_playingRecording(nullptr)
{
  for (int i = 0; i < 1 || i < m_settings->GetTotalTuners(); i++)
  {
    m_dmx.emplace_back(new HTSPDemuxer(m_settings, *this, *m_conn));
  }
  m_dmx_active = m_dmx[0];
}

CTvheadend::~CTvheadend()
{
  Stop();

  for (auto* dmx : m_dmx)
    delete dmx;

  delete m_conn;
  delete m_vfs;
}

void CTvheadend::Start()
{
  CreateThread();
  m_conn->Start();
}

void CTvheadend::Stop()
{
  for (auto* dmx : m_dmx)
    dmx->Close();

  m_conn->Stop();
  StopThread();
}

/* **************************************************************************
 * Miscellaneous
 * *************************************************************************/

ADDON_STATUS CTvheadend::SetInstanceSetting(const std::string& settingName,
                                            const kodi::addon::CSettingValue& settingValue)
{
  return m_settings->SetSetting(settingName, settingValue);
}

PVR_ERROR CTvheadend::GetCapabilities(kodi::addon::PVRCapabilities& capabilities)
{
  capabilities.SetSupportsEPG(true);
  capabilities.SetSupportsTV(true);
  capabilities.SetSupportsRadio(true);
  capabilities.SetSupportsRecordings(true);
  capabilities.SetSupportsRecordingsDelete(true);
  capabilities.SetSupportsRecordingsUndelete(false);
  capabilities.SetSupportsTimers(true);
  capabilities.SetSupportsChannelGroups(true);
  capabilities.SetHandlesInputStream(true);
  capabilities.SetHandlesDemuxing(true);
  capabilities.SetSupportsRecordingEdl(true);
  capabilities.SetSupportsRecordingPlayCount(m_conn->GetProtocol() >= 27 &&
                                             m_settings->GetDvrPlayStatus());
  capabilities.SetSupportsLastPlayedPosition(m_conn->GetProtocol() >= 27 &&
                                             m_settings->GetDvrPlayStatus());
  capabilities.SetSupportsDescrambleInfo(true);
  capabilities.SetSupportsAsyncEPGTransfer(m_settings->GetAsyncEpg());

  if (m_conn->GetProtocol() >= 28)
  {
    capabilities.SetSupportsRecordingsRename(true);

    capabilities.SetSupportsRecordingsLifetimeChange(true);

    /* PVR recording lifetime values and presentation.*/
    std::vector<kodi::addon::PVRTypeIntValue> lifetimeValues;
    GetLivetimeValues(lifetimeValues);
    capabilities.SetRecordingsLifetimeValues(lifetimeValues);
  }

  capabilities.SetSupportsRecordingSize(m_conn->GetProtocol() >= 35);

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::GetDriveSpace(uint64_t& total, uint64_t& used)
{
  std::unique_lock<std::recursive_mutex> lock(m_conn->Mutex());

  htsmsg_t* m = htsmsg_create_map();
  m = m_conn->SendAndWait(lock, "getDiskSpace", m);
  if (!m)
    return PVR_ERROR_SERVER_ERROR;

  int64_t s64 = 0;
  if (htsmsg_get_s64(m, "totaldiskspace", &s64))
    goto error;
  total = s64 / 1024;

  if (htsmsg_get_s64(m, "freediskspace", &s64))
    goto error;
  used = total - (s64 / 1024);

  htsmsg_destroy(m);
  return PVR_ERROR_NO_ERROR;

error:
  htsmsg_destroy(m);
  Logger::Log(LogLevel::LEVEL_ERROR,
              "malformed getDiskSpace response: 'totaldiskspace'/'freediskspace' missing");
  return PVR_ERROR_SERVER_ERROR;
}

std::string CTvheadend::GetImageURL(const char* str)
{
  if (*str != '/')
  {
    if (strncmp(str, "imagecache/", 11) == 0)
      return m_conn->GetWebURL("/%s", str);

    return str;
  }
  else
  {
    return m_conn->GetWebURL("%s", str);
  }
}

void CTvheadend::QueryAvailableProfiles(std::unique_lock<std::recursive_mutex>& lock)
{
  /* Build message */
  htsmsg_t* m = htsmsg_create_map();

  /* Send */
  m = m_conn->SendAndWait0(lock, "getProfiles", m);

  /* Validate */
  if (!m)
    return;

  htsmsg_t* l = htsmsg_get_list(m, "profiles");

  if (!l)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed getProfiles: 'profiles' missing");
    htsmsg_destroy(m);
    return;
  }

  /* Process */
  Logger::Log(LogLevel::LEVEL_INFO, "  Available streaming profiles:");

  htsmsg_field_t* f = nullptr;
  HTSMSG_FOREACH(f, l)
  {
    Profile profile;

    const char* str = htsmsg_get_str(&f->hmf_msg, "uuid");
    if (str)
      profile.SetUuid(str);

    str = htsmsg_get_str(&f->hmf_msg, "name");
    if (str)
      profile.SetName(str);

    str = htsmsg_get_str(&f->hmf_msg, "comment");
    if (str)
      profile.SetComment(str);

    Logger::Log(LogLevel::LEVEL_INFO, "  Name: %s, Comment: %s",
                profile.GetName().c_str(), profile.GetComment().c_str());

    m_profiles.emplace_back(profile);
  }

  htsmsg_destroy(m);
}

bool CTvheadend::HasStreamingProfile(const std::string& streamingProfile) const
{
  return std::find_if(m_profiles.cbegin(), m_profiles.cend(),
                      [&streamingProfile](const Profile& profile) {
                        return profile.GetName() == streamingProfile;
                      }) != m_profiles.cend();
}

/* **************************************************************************
 * Tags
 * *************************************************************************/

PVR_ERROR CTvheadend::GetChannelGroupsAmount(int& amount)
{
  if (!m_asyncState.WaitForState(ASYNC_DVR))
    return PVR_ERROR_FAILED;

  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  amount = m_tags.size();
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::GetChannelGroups(bool radio, kodi::addon::PVRChannelGroupsResultSet& results)
{
  if (!m_asyncState.WaitForState(ASYNC_DVR))
    return PVR_ERROR_FAILED;

  std::vector<kodi::addon::PVRChannelGroup> tags;
  {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    for (const auto& entry : m_tags)
    {
      /* Does group contain channels of the requested type?             */
      /* Note: tvheadend groups can contain both radio and tv channels. */
      /*       Thus, one tvheadend group can 'map' to two Kodi groups.  */
      if (!entry.second.ContainsChannelType(radio ? CHANNEL_TYPE_RADIO : CHANNEL_TYPE_TV,
                                            GetChannels()))
        continue;

      kodi::addon::PVRChannelGroup tag;
      tag.SetGroupName(entry.second.GetName());
      tag.SetIsRadio(radio);
      tag.SetPosition(entry.second.GetIndex());

      tags.emplace_back(tag);
    }
  }

  for (const auto& tag : tags)
  {
    /* Callback. */
    results.Add(tag);
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::GetChannelGroupMembers(const kodi::addon::PVRChannelGroup& group,
                                             kodi::addon::PVRChannelGroupMembersResultSet& results)
{
  if (!m_asyncState.WaitForState(ASYNC_DVR))
    return PVR_ERROR_FAILED;

  std::vector<kodi::addon::PVRChannelGroupMember> gms;
  {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // Find the tag
    const auto it = std::find_if(m_tags.cbegin(), m_tags.cend(), [group](const TagMapEntry& tag) {
      return tag.second.GetName() == group.GetGroupName();
    });

    if (it != m_tags.cend())
    {
      // Find all channels in this group that are of the correct type
      for (const auto& channelId : it->second.GetChannels())
      {
        auto cit = m_channels.find(channelId);

        if (cit != m_channels.cend() &&
            cit->second.GetType() == (group.GetIsRadio() ? CHANNEL_TYPE_RADIO : CHANNEL_TYPE_TV))
        {
          kodi::addon::PVRChannelGroupMember gm;
          gm.SetGroupName(group.GetGroupName());
          gm.SetChannelUniqueId(cit->second.GetId());
          gm.SetChannelNumber(cit->second.GetNum());
          gm.SetSubChannelNumber(cit->second.GetNumMinor());

          gms.emplace_back(gm);
        }
      }
    }
  }

  for (const auto& gm : gms)
  {
    /* Callback. */
    results.Add(gm);
  }

  return PVR_ERROR_NO_ERROR;
}

/* **************************************************************************
 * Channels
 * *************************************************************************/

PVR_ERROR CTvheadend::GetChannelsAmount(int& amount)
{
  if (!m_asyncState.WaitForState(ASYNC_DVR))
    return PVR_ERROR_FAILED;

  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  amount = m_channels.size();
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::GetChannels(bool radio, kodi::addon::PVRChannelsResultSet& results)
{
  if (!m_asyncState.WaitForState(ASYNC_DVR))
    return PVR_ERROR_FAILED;

  std::vector<kodi::addon::PVRChannel> channels;
  {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    for (const auto& entry : m_channels)
    {
      const auto& channel = entry.second;

      if (channel.GetType() != (radio ? CHANNEL_TYPE_RADIO : CHANNEL_TYPE_TV))
        continue;

      kodi::addon::PVRChannel chn;
      chn.SetUniqueId(channel.GetId());
      chn.SetIsRadio(radio);
      chn.SetChannelNumber(channel.GetNum());
      chn.SetSubChannelNumber(channel.GetNumMinor());
      chn.SetEncryptionSystem(channel.GetCaid());
      chn.SetIsHidden(false);
      chn.SetChannelName(channel.GetName());
      chn.SetIconPath(channel.GetIcon());

      channels.emplace_back(chn);
    }
  }

  for (const auto& channel : channels)
  {
    /* Callback. */
    results.Add(channel);
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::GetChannelStreamProperties(const kodi::addon::PVRChannel& channel, std::vector<kodi::addon::PVRStreamProperty>& properties)
{
  if (!m_settings->GetStreamingHTTP())
    return PVR_ERROR_NO_ERROR;

  if (!m_asyncState.WaitForState(ASYNC_DVR))
    return PVR_ERROR_FAILED;

  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  auto it = m_channels.find(channel.GetUniqueId());
  if (it == m_channels.end())
    return PVR_ERROR_FAILED;

  std::string path = "/stream/channelid/" + std::to_string(it->first);
  const std::string streamingProfile = m_settings->GetStreamingProfile();
  if (!streamingProfile.empty())
    path += "?profile=" + streamingProfile;

  const std::string url = m_conn->GetWebURL("%s", path.c_str());

  properties.emplace_back(PVR_STREAM_PROPERTY_STREAMURL, url);
  properties.emplace_back(PVR_STREAM_PROPERTY_ISREALTIMESTREAM, "true");

  return PVR_ERROR_NO_ERROR;
}

/* **************************************************************************
 * Recordings
 * *************************************************************************/

PVR_ERROR CTvheadend::SendDvrDelete(uint32_t id, const char* method)
{
  std::unique_lock<std::recursive_mutex> lock(m_conn->Mutex());

  /* Build message */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", id);

  /* Send and wait a bit longer than usual */
  m = m_conn->SendAndWait(lock, method, m, std::max(30000, m_settings->GetResponseTimeout()));
  if (!m)
    return PVR_ERROR_SERVER_ERROR;

  /* Check for error */
  uint32_t u32 = 0;
  if (htsmsg_get_u32(m, "success", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR,
                "malformed deleteDvrEntry/cancelDvrEntry response: 'success' missing");
    u32 = PVR_ERROR_FAILED;
  }
  htsmsg_destroy(m);

  return u32 > 0 ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
}

PVR_ERROR CTvheadend::SendDvrUpdate(htsmsg_t* m)
{
  /* Send and Wait */
  {
    std::unique_lock<std::recursive_mutex> lock(m_conn->Mutex());
    m = m_conn->SendAndWait(lock, "updateDvrEntry", m);
  }

  if (!m)
    return PVR_ERROR_SERVER_ERROR;

  /* Check for error */
  uint32_t u32 = 0;
  if (htsmsg_get_u32(m, "success", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed updateDvrEntry response: 'success' missing");
    u32 = PVR_ERROR_FAILED;
  }
  htsmsg_destroy(m);

  return u32 > 0 ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
}

PVR_ERROR CTvheadend::GetRecordingsAmount(bool deleted, int& amount)
{
  if (!m_asyncState.WaitForState(ASYNC_EPG))
    return PVR_ERROR_FAILED;

  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  amount = std::count_if(m_recordings.cbegin(), m_recordings.cend(),
                         [](const RecordingMapEntry& entry) { return entry.second.IsRecording(); });
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::GetRecordings(bool deleted, kodi::addon::PVRRecordingsResultSet& results)
{
  if (!m_asyncState.WaitForState(ASYNC_EPG))
    return PVR_ERROR_FAILED;

  std::vector<kodi::addon::PVRRecording> recs;
  {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    char buf[128];

    for (const auto& entry : m_recordings)
    {
      const auto& recording = entry.second;

      if (!recording.IsRecording())
        continue;

      /* Setup entry */
      kodi::addon::PVRRecording rec;

      /* Channel icon */
      const auto& cit = m_channels.find(recording.GetChannel());
      if (cit != m_channels.end())
        rec.SetIconPath(cit->second.GetIcon());

      /* Channel name */
      rec.SetChannelName(recording.GetChannelName());

      /* Thumbnail image */
      rec.SetThumbnailPath(recording.GetImage());

      /* Fanart image */
      rec.SetFanartPath(recording.GetFanartImage());

      /* ID */
      rec.SetRecordingId(std::to_string(recording.GetId()));

      /* Title */
      rec.SetTitle(recording.GetTitle());

      /* Subtitle */
      rec.SetEpisodeName(recording.GetSubtitle());

      /* season/episode (tvh 4.3+) */
      rec.SetSeriesNumber(recording.GetSeason());
      rec.SetEpisodeNumber(recording.GetEpisode());

      /* Description */
      rec.SetPlot(recording.GetDescription());

      /* Genre */
      rec.SetGenreType(recording.GetGenreType());
      rec.SetGenreSubType(recording.GetGenreSubType());

      /* Time/Duration (prefer real start/stop time over scheduled start/stop time if possible.) */
      int64_t start;
      int64_t stop;
      if (recording.GetFilesStart() > 0)
      {
        start = recording.GetFilesStart();

        if (recording.GetFilesStop() > 0) // finished / in progress?
          stop = recording.GetFilesStop();
        else
          stop = recording.GetStop() + recording.GetStopExtra() * 60;
      }
      else
      {
        start = recording.GetStart() - recording.GetStartExtra() * 60;
        stop = recording.GetStop() + recording.GetStopExtra() * 60;
      }

      rec.SetRecordingTime(static_cast<time_t>(start));
      rec.SetDuration(static_cast<int>(stop - start));

      /* File size */
      rec.SetSizeInBytes(recording.GetFilesSize());

      /* Priority */
      rec.SetPriority(recording.GetPriority());

      /* Lifetime (based on retention or removal) */
      rec.SetLifetime(recording.GetLifetime());

      /* Play status */
      rec.SetPlayCount(recording.GetPlayCount());
      rec.SetLastPlayedPosition(recording.GetPlayPosition());

      /* Directory */
      // TODO: Move this logic to GetPath(), alternatively GetMangledPath()
      if (recording.GetPath() != "")
      {
        size_t idx = recording.GetPath().rfind("/");
        if (idx == 0 || idx == std::string::npos)
          rec.SetDirectory("/");
        else
        {
          std::string d = recording.GetPath().substr(0, idx);
          if (d[0] != '/')
            d = "/" + d;
          rec.SetDirectory(d);
        }
      }

      /* EPG event id */
      rec.SetEPGEventId(recording.GetEventId());

      /* channel id */
      rec.SetChannelUid(recording.GetChannel() > 0 ? recording.GetChannel()
                                                   : PVR_CHANNEL_INVALID_UID);

      /* channel type */
      switch (recording.GetChannelType())
      {
        case CHANNEL_TYPE_TV:
          rec.SetChannelType(PVR_RECORDING_CHANNEL_TYPE_TV);
          break;
        case CHANNEL_TYPE_RADIO:
          rec.SetChannelType(PVR_RECORDING_CHANNEL_TYPE_RADIO);
          break;
        case CHANNEL_TYPE_OTHER:
        default:
          rec.SetChannelType(PVR_RECORDING_CHANNEL_TYPE_UNKNOWN);
          break;
      }

      recs.emplace_back(rec);
    }
  }

  for (const auto& rec : recs)
  {
    /* Callback. */
    results.Add(rec);
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::GetRecordingEdl(const kodi::addon::PVRRecording& rec,
                                      std::vector<kodi::addon::PVREDLEntry>& edl)
{
  /* Build request */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", std::stoul(rec.GetRecordingId()));

  Logger::Log(LogLevel::LEVEL_DEBUG, "dvr get cutpoints id=%s", rec.GetRecordingId().c_str());

  /* Send and Wait */
  {
    std::unique_lock<std::recursive_mutex> lock(m_conn->Mutex());

    m = m_conn->SendAndWait(lock, "getDvrCutpoints", m);
    if (!m)
      return PVR_ERROR_SERVER_ERROR;
  }

  /* Check for optional "cutpoints" reply message field */
  htsmsg_t* list = htsmsg_get_list(m, "cutpoints");
  if (!list)
  {
    htsmsg_destroy(m);
    return PVR_ERROR_NO_ERROR;
  }

  /* Process */
  htsmsg_field_t* f = nullptr;
  HTSMSG_FOREACH(f, list)
  {
    uint32_t start = 0;
    uint32_t end = 0;
    uint32_t type = 0;

    if (f->hmf_type != HMF_MAP)
      continue;

    /* Get fields */
    if (htsmsg_get_u32(&f->hmf_msg, "start", &start) || htsmsg_get_u32(&f->hmf_msg, "end", &end) ||
        htsmsg_get_u32(&f->hmf_msg, "type", &type))
    {
      Logger::Log(LogLevel::LEVEL_ERROR,
                  "malformed getDvrCutpoints response: invalid EDL entry, will ignore");
      continue;
    }

    /* Build entry */
    kodi::addon::PVREDLEntry entry;

    entry.SetStart(start);
    entry.SetEnd(end);
    switch (type)
    {
      case DVR_ACTION_TYPE_CUT:
        entry.SetType(PVR_EDL_TYPE_CUT);
        break;
      case DVR_ACTION_TYPE_MUTE:
        entry.SetType(PVR_EDL_TYPE_MUTE);
        break;
      case DVR_ACTION_TYPE_SCENE:
        entry.SetType(PVR_EDL_TYPE_SCENE);
        break;
      case DVR_ACTION_TYPE_COMBREAK:
      default:
        entry.SetType(PVR_EDL_TYPE_COMBREAK);
        break;
    }
    edl.emplace_back(entry);

    Logger::Log(LogLevel::LEVEL_DEBUG, "edl start:%d end:%d action:%d", start, end, type);
  }

  htsmsg_destroy(m);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::DeleteRecording(const kodi::addon::PVRRecording& rec)
{
  return SendDvrDelete(std::stoul(rec.GetRecordingId()), "deleteDvrEntry");
}

PVR_ERROR CTvheadend::RenameRecording(const kodi::addon::PVRRecording& rec)
{
  if (m_conn->GetProtocol() < 28)
    return PVR_ERROR_NOT_IMPLEMENTED;

  /* Build message */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", std::stoul(rec.GetRecordingId()));
  htsmsg_add_str(m, "title", rec.GetTitle().c_str());

  return SendDvrUpdate(m);
}

PVR_ERROR CTvheadend::SetRecordingLifetime(const kodi::addon::PVRRecording& rec)
{
  if (m_conn->GetProtocol() < 28)
    return PVR_ERROR_NOT_IMPLEMENTED;

  Logger::Log(LogLevel::LEVEL_DEBUG, "Setting lifetime to %i for recording %s", rec.GetLifetime(),
              rec.GetRecordingId().c_str());

  /* Build message */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", std::stoul(rec.GetRecordingId()));
  htsmsg_add_u32(m, "removal", LifetimeMapper::KodiToTvh(rec.GetLifetime())); // remove from disk

  return SendDvrUpdate(m);
}

PVR_ERROR CTvheadend::SetRecordingPlayCount(const kodi::addon::PVRRecording& rec, int playCount)
{
  if (m_conn->GetProtocol() < 27 || !m_settings->GetDvrPlayStatus())
    return PVR_ERROR_NOT_IMPLEMENTED;

  Logger::Log(LogLevel::LEVEL_DEBUG, "Setting play count to %i for recording %s", playCount,
              rec.GetRecordingId().c_str());

  /* Build message */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", std::stoul(rec.GetRecordingId()));
  htsmsg_add_u32(m, "playcount", playCount);
  return SendDvrUpdate(m);
}

PVR_ERROR CTvheadend::SetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& rec,
                                                     int playPosition)
{
  if (m_conn->GetProtocol() < 27 || !m_settings->GetDvrPlayStatus())
    return PVR_ERROR_NOT_IMPLEMENTED;

  Logger::Log(LogLevel::LEVEL_DEBUG, "Setting play position to %i for recording %s", playPosition,
              rec.GetRecordingId().c_str());

  /* Build message */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", std::stoul(rec.GetRecordingId()));
  htsmsg_add_u32(m, "playposition",
                 playPosition >= 0 ? playPosition : 0); // Kodi uses -1 when fully watched
  return SendDvrUpdate(m);
}

PVR_ERROR CTvheadend::GetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& rec,
                                                     int& playPosition)
{
  if (m_conn->GetProtocol() < 27 || !m_settings->GetDvrPlayStatus())
    return PVR_ERROR_NOT_IMPLEMENTED;

  if (!m_asyncState.WaitForState(ASYNC_EPG))
    return PVR_ERROR_FAILED;

  Logger::Log(LogLevel::LEVEL_INFO, "xxxx");
  playPosition = 0;

//  std::lock_guard<std::recursive_mutex> lock(m_mutex);
//
//  const auto& it = m_recordings.find(std::stoul(rec.GetRecordingId()));
//  if (it != m_recordings.end() && it->second.IsRecording())
//  {
//    Logger::Log(LogLevel::LEVEL_DEBUG, "Getting play position %i for recording %s",
//                it->second.GetPlayPosition(), rec.GetTitle().c_str());
//    playPosition = it->second.GetPlayPosition();
//  }

  return PVR_ERROR_NO_ERROR;
}

namespace
{
struct TimerType : kodi::addon::PVRTimerType
{
  TimerType(const std::shared_ptr<InstanceSettings>& settings,
            unsigned int id,
            unsigned int attributes,
            const std::string& description,
            const std::vector<kodi::addon::PVRTypeIntValue>& priorityValues =
                std::vector<kodi::addon::PVRTypeIntValue>(),
            const std::vector<kodi::addon::PVRTypeIntValue>& lifetimeValues =
                std::vector<kodi::addon::PVRTypeIntValue>(),
            const std::vector<kodi::addon::PVRTypeIntValue>& dupEpisodesValues =
                std::vector<kodi::addon::PVRTypeIntValue>())
  {
    SetId(id);
    SetAttributes(attributes);
    SetDescription(description);
    SetPriorities(priorityValues, settings->GetDvrPriority());
    SetLifetimes(lifetimeValues, LifetimeMapper::TvhToKodi(settings->GetDvrLifetime()));
    SetPreventDuplicateEpisodes(dupEpisodesValues, settings->GetDvrDupdetect());
  }
};

} // unnamed namespace

void CTvheadend::GetLivetimeValues(std::vector<kodi::addon::PVRTypeIntValue>& lifetimeValues) const
{
  lifetimeValues = {
      {LifetimeMapper::TvhToKodi(DVR_RET_DVRCONFIG), kodi::addon::GetLocalizedString(30390)},
      {LifetimeMapper::TvhToKodi(DVR_RET_1DAY), kodi::addon::GetLocalizedString(30375)},
      {LifetimeMapper::TvhToKodi(DVR_RET_3DAY), kodi::addon::GetLocalizedString(30376)},
      {LifetimeMapper::TvhToKodi(DVR_RET_5DAY), kodi::addon::GetLocalizedString(30377)},
      {LifetimeMapper::TvhToKodi(DVR_RET_1WEEK), kodi::addon::GetLocalizedString(30378)},
      {LifetimeMapper::TvhToKodi(DVR_RET_2WEEK), kodi::addon::GetLocalizedString(30379)},
      {LifetimeMapper::TvhToKodi(DVR_RET_3WEEK), kodi::addon::GetLocalizedString(30380)},
      {LifetimeMapper::TvhToKodi(DVR_RET_1MONTH), kodi::addon::GetLocalizedString(30381)},
      {LifetimeMapper::TvhToKodi(DVR_RET_2MONTH), kodi::addon::GetLocalizedString(30382)},
      {LifetimeMapper::TvhToKodi(DVR_RET_3MONTH), kodi::addon::GetLocalizedString(30383)},
      {LifetimeMapper::TvhToKodi(DVR_RET_6MONTH), kodi::addon::GetLocalizedString(30384)},
      {LifetimeMapper::TvhToKodi(DVR_RET_1YEAR), kodi::addon::GetLocalizedString(30385)},
      {LifetimeMapper::TvhToKodi(DVR_RET_2YEARS), kodi::addon::GetLocalizedString(30386)},
      {LifetimeMapper::TvhToKodi(DVR_RET_3YEARS), kodi::addon::GetLocalizedString(30387)},
      {LifetimeMapper::TvhToKodi(DVR_RET_SPACE), kodi::addon::GetLocalizedString(30388)},
      {LifetimeMapper::TvhToKodi(DVR_RET_FOREVER), kodi::addon::GetLocalizedString(30389)},
  };
}

PVR_ERROR CTvheadend::GetTimerTypes(std::vector<kodi::addon::PVRTimerType>& types)
{
  /* PVR_Timer.iPriority values and presentation.*/
  static std::vector<kodi::addon::PVRTypeIntValue> priorityValues;
  if (priorityValues.size() == 0)
  {
    priorityValues = {
        {DVR_PRIO_DEFAULT, kodi::addon::GetLocalizedString(30368)},
        {DVR_PRIO_UNIMPORTANT, kodi::addon::GetLocalizedString(30355)},
        {DVR_PRIO_LOW, kodi::addon::GetLocalizedString(30354)},
        {DVR_PRIO_NORMAL, kodi::addon::GetLocalizedString(30353)},
        {DVR_PRIO_HIGH, kodi::addon::GetLocalizedString(30352)},
        {DVR_PRIO_IMPORTANT, kodi::addon::GetLocalizedString(30351)},
    };
  }

  /* PVR_Timer.iPreventDuplicateEpisodes values and presentation.*/
  std::vector<kodi::addon::PVRTypeIntValue> deDupValues = {
      {DVR_AUTOREC_RECORD_ALL, kodi::addon::GetLocalizedString(30356)},
      {DVR_AUTOREC_RECORD_DIFFERENT_EPISODE_NUMBER, kodi::addon::GetLocalizedString(30357)},
      {DVR_AUTOREC_RECORD_DIFFERENT_SUBTITLE, kodi::addon::GetLocalizedString(30358)},
      {DVR_AUTOREC_RECORD_DIFFERENT_DESCRIPTION, kodi::addon::GetLocalizedString(30359)},
  };

  if (m_conn->GetProtocol() >= 27)
    deDupValues.emplace_back(DVR_AUTOREC_RECORD_ONCE_PER_MONTH,
                             kodi::addon::GetLocalizedString(30370));

  deDupValues.emplace_back(DVR_AUTOREC_RECORD_ONCE_PER_WEEK,
                           kodi::addon::GetLocalizedString(30360));
  deDupValues.emplace_back(DVR_AUTOREC_RECORD_ONCE_PER_DAY, kodi::addon::GetLocalizedString(30361));

  deDupValues.emplace_back(DVR_AUTOREC_LRECORD_DIFFERENT_EPISODE_NUMBER,
                           kodi::addon::GetLocalizedString(30362));
  deDupValues.emplace_back(DVR_AUTOREC_LRECORD_DIFFERENT_SUBTITLE,
                           kodi::addon::GetLocalizedString(30363));
  deDupValues.emplace_back(DVR_AUTOREC_LRECORD_DIFFERENT_TITLE,
                           kodi::addon::GetLocalizedString(30364));
  deDupValues.emplace_back(DVR_AUTOREC_LRECORD_DIFFERENT_DESCRIPTION,
                           kodi::addon::GetLocalizedString(30365));

  if (m_conn->GetProtocol() >= 27)
    deDupValues.emplace_back(DVR_AUTOREC_LRECORD_ONCE_PER_MONTH,
                             kodi::addon::GetLocalizedString(30371));

  deDupValues.emplace_back(DVR_AUTOREC_LRECORD_ONCE_PER_WEEK,
                           kodi::addon::GetLocalizedString(30366));
  deDupValues.emplace_back(DVR_AUTOREC_LRECORD_ONCE_PER_DAY,
                           kodi::addon::GetLocalizedString(30367));

  if (m_conn->GetProtocol() >= 31)
    deDupValues.emplace_back(DVR_AUTOREC_RECORD_UNIQUE, kodi::addon::GetLocalizedString(30372));

  /* PVR_Timer.iLifetime values and presentation.*/
  std::vector<kodi::addon::PVRTypeIntValue> lifetimeValues;
  GetLivetimeValues(lifetimeValues);

  unsigned int TIMER_ONCE_MANUAL_ATTRIBS =
      PVR_TIMER_TYPE_IS_MANUAL | PVR_TIMER_TYPE_SUPPORTS_CHANNELS |
      PVR_TIMER_TYPE_SUPPORTS_START_TIME | PVR_TIMER_TYPE_SUPPORTS_END_TIME |
      PVR_TIMER_TYPE_SUPPORTS_PRIORITY | PVR_TIMER_TYPE_SUPPORTS_LIFETIME |
      PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE;

  unsigned int TIMER_ONCE_EPG_ATTRIBS =
      PVR_TIMER_TYPE_SUPPORTS_CHANNELS | PVR_TIMER_TYPE_SUPPORTS_START_TIME |
      PVR_TIMER_TYPE_SUPPORTS_END_TIME | PVR_TIMER_TYPE_REQUIRES_EPG_TAG_ON_CREATE |
      PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN | PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
      PVR_TIMER_TYPE_SUPPORTS_LIFETIME | PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE;

  /* Timer types definition. */

  /* One-shot manual (time and channel based) */
  types.emplace_back(TimerType(
      /* Settings */
      m_settings,
      /* Type id. */
      TIMER_ONCE_MANUAL,
      /* Attributes. */
      TIMER_ONCE_MANUAL_ATTRIBS,
      /* Let Kodi generate the description. */
      "",
      /* Values definitions for priorities. */
      priorityValues,
      /* Values definitions for lifetime. */
      lifetimeValues));

  /* One-shot epg based */
  types.emplace_back(TimerType(
      /* Settings */
      m_settings,
      /* Type id. */
      TIMER_ONCE_EPG,
      /* Attributes. */
      TIMER_ONCE_EPG_ATTRIBS,
      /* Let Kodi generate the description. */
      "",
      /* Values definitions for priorities. */
      priorityValues,
      /* Values definitions for lifetime. */
      lifetimeValues));

  /* Read-only one-shot for timers generated by timerec */
  types.emplace_back(TimerType(
      /* Settings */
      m_settings,
      /* Type id. */
      TIMER_ONCE_CREATED_BY_TIMEREC,
      /* Attributes. */
      TIMER_ONCE_MANUAL_ATTRIBS | PVR_TIMER_TYPE_IS_READONLY | PVR_TIMER_TYPE_FORBIDS_NEW_INSTANCES,
      /* Description. */
      kodi::addon::GetLocalizedString(30350), // "One Time (Scheduled by timer rule)"
      /* Values definitions for priorities. */
      priorityValues,
      /* Values definitions for lifetime. */
      lifetimeValues));

  /* Read-only one-shot for timers generated by autorec */
  types.emplace_back(TimerType(
      /* Settings */
      m_settings,
      /* Type id. */
      TIMER_ONCE_CREATED_BY_AUTOREC,
      /* Attributes. */
      TIMER_ONCE_EPG_ATTRIBS | PVR_TIMER_TYPE_IS_READONLY | PVR_TIMER_TYPE_FORBIDS_NEW_INSTANCES,
      /* Description. */
      kodi::addon::GetLocalizedString(30350), // "One Time (Scheduled by timer rule)"
      /* Values definitions for priorities. */
      priorityValues,
      /* Values definitions for lifetime. */
      lifetimeValues));

  /* Repeating manual (time and channel based) - timerec */
  types.emplace_back(TimerType(
      /* Settings */
      m_settings,
      /* Type id. */
      TIMER_REPEATING_MANUAL,
      /* Attributes. */
      PVR_TIMER_TYPE_IS_MANUAL | PVR_TIMER_TYPE_IS_REPEATING |
          PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE | PVR_TIMER_TYPE_SUPPORTS_CHANNELS |
          PVR_TIMER_TYPE_SUPPORTS_START_TIME | PVR_TIMER_TYPE_SUPPORTS_END_TIME |
          PVR_TIMER_TYPE_SUPPORTS_WEEKDAYS | PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
          PVR_TIMER_TYPE_SUPPORTS_LIFETIME | PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS,
      /* Let Kodi generate the description. */
      "",
      /* Values definitions for priorities. */
      priorityValues,
      /* Values definitions for lifetime. */
      lifetimeValues));

  if (m_conn->GetProtocol() >= 29)
  {
    unsigned int TIMER_REPEATING_SERIESLINK_ATTRIBS =
        PVR_TIMER_TYPE_IS_REPEATING | PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE |
        PVR_TIMER_TYPE_SUPPORTS_CHANNELS | PVR_TIMER_TYPE_SUPPORTS_START_TIME |
        PVR_TIMER_TYPE_SUPPORTS_START_ANYTIME | PVR_TIMER_TYPE_SUPPORTS_WEEKDAYS |
        PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN | PVR_TIMER_TYPE_SUPPORTS_PRIORITY |
        PVR_TIMER_TYPE_SUPPORTS_LIFETIME | PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS |
        PVR_TIMER_TYPE_SUPPORTS_ANY_CHANNEL | PVR_TIMER_TYPE_REQUIRES_EPG_SERIESLINK_ON_CREATE;

    if (!m_settings->GetAutorecApproxTime())
    {
      /* We need the end time to represent the end of the tvh starting window */
      TIMER_REPEATING_SERIESLINK_ATTRIBS |= PVR_TIMER_TYPE_SUPPORTS_END_TIME;
      TIMER_REPEATING_SERIESLINK_ATTRIBS |= PVR_TIMER_TYPE_SUPPORTS_END_ANYTIME;
    }

    /* Repeating epg based - series link autorec */
    types.emplace_back(TimerType(
        /* Settings */
        m_settings,
        /* Type id. */
        TIMER_REPEATING_SERIESLINK,
        /* Attributes. */
        TIMER_REPEATING_SERIESLINK_ATTRIBS,
        /* Description. */
        kodi::addon::GetLocalizedString(30369), // "Timer rule (series link)"
        /* Values definitions for priorities. */
        priorityValues,
        /* Values definitions for lifetime. */
        lifetimeValues));
  }

  unsigned int TIMER_REPEATING_EPG_ATTRIBS =
      PVR_TIMER_TYPE_IS_REPEATING | PVR_TIMER_TYPE_SUPPORTS_ENABLE_DISABLE |
      PVR_TIMER_TYPE_SUPPORTS_TITLE_EPG_MATCH | PVR_TIMER_TYPE_SUPPORTS_CHANNELS |
      PVR_TIMER_TYPE_SUPPORTS_START_TIME | PVR_TIMER_TYPE_SUPPORTS_START_ANYTIME |
      PVR_TIMER_TYPE_SUPPORTS_WEEKDAYS | PVR_TIMER_TYPE_SUPPORTS_START_END_MARGIN |
      PVR_TIMER_TYPE_SUPPORTS_PRIORITY | PVR_TIMER_TYPE_SUPPORTS_LIFETIME |
      PVR_TIMER_TYPE_SUPPORTS_RECORDING_FOLDERS | PVR_TIMER_TYPE_SUPPORTS_ANY_CHANNEL |
      PVR_TIMER_TYPE_SUPPORTS_FULLTEXT_EPG_MATCH | PVR_TIMER_TYPE_SUPPORTS_RECORD_ONLY_NEW_EPISODES;

  if (!m_settings->GetAutorecApproxTime())
  {
    /* We need the end time to represent the end of the tvh starting window */
    TIMER_REPEATING_EPG_ATTRIBS |= PVR_TIMER_TYPE_SUPPORTS_END_TIME;
    TIMER_REPEATING_EPG_ATTRIBS |= PVR_TIMER_TYPE_SUPPORTS_END_ANYTIME;
  }

  /* Repeating epg based - autorec */
  types.emplace_back(TimerType(
      /* Settings */
      m_settings,
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
      deDupValues));

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::GetTimersAmount(int& amount)
{
  if (!m_asyncState.WaitForState(ASYNC_EPG))
    return PVR_ERROR_FAILED;

  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  // Normal timers
  amount = std::count_if(m_recordings.cbegin(), m_recordings.cend(),
                         [](const RecordingMapEntry& entry) { return entry.second.IsTimer(); });

  // Repeating timers
  amount += m_timeRecordings.GetTimerecTimerCount();
  amount += m_autoRecordings.GetAutorecTimerCount();

  return PVR_ERROR_NO_ERROR;
}

bool CTvheadend::CreateTimer(const Recording& tvhTmr, kodi::addon::PVRTimer& tmr)
{
  tmr.SetClientIndex(tvhTmr.GetId());
  tmr.SetClientChannelUid((tvhTmr.GetChannel() > 0) ? tvhTmr.GetChannel()
                                                    : PVR_CHANNEL_INVALID_UID);
  tmr.SetStartTime(static_cast<time_t>(tvhTmr.GetStart()));
  tmr.SetEndTime(static_cast<time_t>(tvhTmr.GetStop()));
  tmr.SetTitle(tvhTmr.GetTitle());
  tmr.SetEPGSearchString(""); // n/a for one-shot timers
  tmr.SetDirectory(""); // n/a for one-shot timers
  tmr.SetSummary(tvhTmr.GetDescription());
  tmr.SetState(!tvhTmr.IsEnabled() ? PVR_TIMER_STATE_DISABLED : tvhTmr.GetState());
  tmr.SetPriority(tvhTmr.GetPriority());
  tmr.SetLifetime(tvhTmr.GetLifetime());
  tmr.SetTimerType(tvhTmr.GetTimerType());
  tmr.SetMaxRecordings(0); // not supported by tvh
  tmr.SetRecordingGroup(0); // not supported by tvh
  tmr.SetPreventDuplicateEpisodes(0); // n/a for one-shot timers
  tmr.SetFirstDay(0); // not supported by tvh
  tmr.SetWeekdays(PVR_WEEKDAY_NONE); // n/a for one-shot timers
  tmr.SetEPGUid((tvhTmr.GetEventId() > 0) ? tvhTmr.GetEventId() : PVR_TIMER_NO_EPG_UID);
  tmr.SetMarginStart(static_cast<unsigned int>(tvhTmr.GetStartExtra()));
  tmr.SetMarginEnd(static_cast<unsigned int>(tvhTmr.GetStopExtra()));
  tmr.SetGenreType(0); // not supported by tvh?
  tmr.SetGenreSubType(0); // not supported by tvh?
  tmr.SetFullTextEpgSearch(false); // n/a for one-shot timers
  tmr.SetParentClientIndex(
      tmr.GetTimerType() == TIMER_ONCE_CREATED_BY_TIMEREC
          ? m_timeRecordings.GetTimerIntIdFromStringId(tvhTmr.GetTimerecId())
          : tmr.GetTimerType() == TIMER_ONCE_CREATED_BY_AUTOREC
                ? m_autoRecordings.GetTimerIntIdFromStringId(tvhTmr.GetAutorecId())
                : 0);
  return true;
}

PVR_ERROR CTvheadend::GetTimers(kodi::addon::PVRTimersResultSet& results)
{
  if (!m_asyncState.WaitForState(ASYNC_EPG))
    return PVR_ERROR_FAILED;

  std::vector<kodi::addon::PVRTimer> timers;
  {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    /* One-shot timers */
    for (const auto& entry : m_recordings)
    {
      const auto& recording = entry.second;

      if (!recording.IsTimer())
        continue;

      /* Setup entry */
      kodi::addon::PVRTimer tmr;
      if (CreateTimer(recording, tmr))
        timers.emplace_back(tmr);
    }

    /* Time-based repeating timers */
    m_timeRecordings.GetTimerecTimers(timers);

    /* EPG-query-based repeating timers */
    m_autoRecordings.GetAutorecTimers(timers);
  }

  for (const auto& timer : timers)
  {
    /* Callback. */
    results.Add(timer);
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::AddTimer(const kodi::addon::PVRTimer& timer)
{
  if ((timer.GetTimerType() == TIMER_ONCE_MANUAL) || (timer.GetTimerType() == TIMER_ONCE_EPG))
  {
    /* one shot timer */

    /* Build message */
    htsmsg_t* m = htsmsg_create_map();

    int64_t start = timer.GetStartTime();
    if (timer.GetEPGUid() > PVR_TIMER_NO_EPG_UID && timer.GetTimerType() == TIMER_ONCE_EPG &&
        start != 0)
    {
      /* EPG-based timer */
      htsmsg_add_u32(m, "eventId", timer.GetEPGUid());
    }
    else
    {
      /* manual timer */
      htsmsg_add_str(m, "title", timer.GetTitle().c_str());

      if (start == 0)
      {
        /* Instant timer. Adjust start time to 'now'. */
        start = std::time(nullptr);
      }

      htsmsg_add_s64(m, "start", start);
      htsmsg_add_s64(m, "stop", timer.GetEndTime());
      htsmsg_add_u32(m, "channelId", timer.GetClientChannelUid());
      htsmsg_add_str(m, "description", timer.GetSummary().c_str());
    }

    htsmsg_add_u32(m, "enabled", timer.GetState() == PVR_TIMER_STATE_DISABLED ? 0 : 1);
    htsmsg_add_s64(m, "startExtra", timer.GetMarginStart());
    htsmsg_add_s64(m, "stopExtra", timer.GetMarginEnd());
    htsmsg_add_u32(m, "removal",
                   LifetimeMapper::KodiToTvh(timer.GetLifetime())); // remove from disk
    htsmsg_add_u32(m, "priority", timer.GetPriority());

    /* Send and Wait */
    {
      std::unique_lock<std::recursive_mutex> lock(m_conn->Mutex());
      m = m_conn->SendAndWait(lock, "addDvrEntry", m);
    }

    if (!m)
      return PVR_ERROR_SERVER_ERROR;

    /* Check for error */
    uint32_t u32 = 0;
    if (htsmsg_get_u32(m, "success", &u32))
    {
      Logger::Log(LogLevel::LEVEL_ERROR, "malformed addDvrEntry response: 'success' missing");
      u32 = PVR_ERROR_FAILED;
    }
    htsmsg_destroy(m);

    return u32 > 0 ? PVR_ERROR_NO_ERROR : PVR_ERROR_FAILED;
  }
  else if (timer.GetTimerType() == TIMER_REPEATING_MANUAL)
  {
    /* time-based repeating timer */
    return m_timeRecordings.SendTimerecAdd(timer);
  }
  else if (timer.GetTimerType() == TIMER_REPEATING_EPG ||
           timer.GetTimerType() == TIMER_REPEATING_SERIESLINK)
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

PVR_ERROR CTvheadend::DeleteTimer(const kodi::addon::PVRTimer& timer, bool)
{
  if (!m_asyncState.WaitForState(ASYNC_EPG))
    return PVR_ERROR_FAILED;

  {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    const auto& it = m_recordings.find(timer.GetClientIndex());
    if (it != m_recordings.end() && it->second.IsRecording())
    {
      // gracefully stop the recording (mark as success in tvh)
      return SendDvrDelete(timer.GetClientIndex(), "stopDvrEntry");
    }
  }

  if ((timer.GetTimerType() == TIMER_ONCE_MANUAL) || (timer.GetTimerType() == TIMER_ONCE_EPG))
  {
    /* one shot timer */
    return SendDvrDelete(timer.GetClientIndex(), "cancelDvrEntry");
  }
  else if (timer.GetTimerType() == TIMER_REPEATING_MANUAL)
  {
    /* time-based repeating timer */
    return m_timeRecordings.SendTimerecDelete(timer);
  }
  else if (timer.GetTimerType() == TIMER_REPEATING_EPG ||
           timer.GetTimerType() == TIMER_REPEATING_SERIESLINK)
  {
    /* EPG-query-based repeating timer */
    return m_autoRecordings.SendAutorecDelete(timer);
  }
  else if ((timer.GetTimerType() == TIMER_ONCE_CREATED_BY_TIMEREC) ||
           (timer.GetTimerType() == TIMER_ONCE_CREATED_BY_AUTOREC))
  {
    /* Read-only timer created by autorec or timerec */
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

PVR_ERROR CTvheadend::UpdateTimer(const kodi::addon::PVRTimer& timer)
{
  if ((timer.GetTimerType() == TIMER_ONCE_MANUAL) || (timer.GetTimerType() == TIMER_ONCE_EPG))
  {
    /* one shot timer */

    /* Build message */
    htsmsg_t* m = htsmsg_create_map();
    htsmsg_add_u32(m, "id", timer.GetClientIndex());
    htsmsg_add_u32(m, "channelId", timer.GetClientChannelUid());
    htsmsg_add_str(m, "title", timer.GetTitle().c_str());
    htsmsg_add_u32(m, "enabled", timer.GetState() == PVR_TIMER_STATE_DISABLED ? 0 : 1);

    int64_t start = timer.GetStartTime();
    if (start == 0)
    {
      /* Instant timer. Adjust start time to 'now'. */
      start = std::time(nullptr);
    }

    htsmsg_add_s64(m, "start", start);
    htsmsg_add_s64(m, "stop", timer.GetEndTime());
    htsmsg_add_str(m, "description", timer.GetSummary().c_str());
    htsmsg_add_s64(m, "startExtra", timer.GetMarginStart());
    htsmsg_add_s64(m, "stopExtra", timer.GetMarginEnd());
    htsmsg_add_u32(m, "removal",
                   LifetimeMapper::KodiToTvh(timer.GetLifetime())); // remove from disk
    htsmsg_add_u32(m, "priority", timer.GetPriority());

    return SendDvrUpdate(m);
  }
  else if (timer.GetTimerType() == TIMER_REPEATING_MANUAL)
  {
    /* time-based repeating timer */
    return m_timeRecordings.SendTimerecUpdate(timer);
  }
  else if (timer.GetTimerType() == TIMER_REPEATING_EPG ||
           timer.GetTimerType() == TIMER_REPEATING_SERIESLINK)
  {
    /* EPG-query-based repeating timers */
    return m_autoRecordings.SendAutorecUpdate(timer);
  }
  else if ((timer.GetTimerType() == TIMER_ONCE_CREATED_BY_TIMEREC) ||
           (timer.GetTimerType() == TIMER_ONCE_CREATED_BY_AUTOREC))
  {
    if (!m_asyncState.WaitForState(ASYNC_EPG))
      return PVR_ERROR_FAILED;

    /* Read-only timer created by autorec or timerec */
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    const auto& it = m_recordings.find(timer.GetClientIndex());
    if (it != m_recordings.end() &&
        (it->second.IsEnabled() == (timer.GetState() == PVR_TIMER_STATE_DISABLED)))
    {
      /* This is actually a request to enable/disable a timer. */
      htsmsg_t* m = htsmsg_create_map();
      htsmsg_add_u32(m, "id", timer.GetClientIndex());
      htsmsg_add_u32(m, "enabled", timer.GetState() == PVR_TIMER_STATE_DISABLED ? 0 : 1);
      return SendDvrUpdate(m);
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

void CTvheadend::CreateEvent(const Event& event, kodi::addon::PVREPGTag& epg)
{
  epg.SetUniqueBroadcastId(event.GetId());
  epg.SetUniqueChannelId(event.GetChannel());
  epg.SetTitle(event.GetTitle());
  epg.SetStartTime(event.GetStart());
  epg.SetEndTime(event.GetStop());
  epg.SetPlotOutline(event.GetSummary());
  epg.SetPlot(event.GetDesc());
  epg.SetOriginalTitle(""); /* not supported by tvh */
  epg.SetCast(event.GetCast());
  epg.SetDirector(event.GetDirectors());
  epg.SetWriter(event.GetWriters());
  epg.SetYear(event.GetYear());
  epg.SetIMDBNumber(""); /* not supported by tvh */
  epg.SetIconPath(event.GetImage());
  epg.SetGenreType(event.GetGenreType());
  epg.SetGenreSubType(event.GetGenreSubType());
  if (epg.GetGenreType() == 0)
  {
    const std::string& categories(event.GetCategories());
    if (!categories.empty())
    {
      epg.SetGenreType(EPG_GENRE_USE_STRING);
      epg.SetGenreDescription(categories);
    }
  }
  epg.SetFirstAired(event.GetAired());
  epg.SetParentalRating(event.GetAge());
  epg.SetStarRating(event.GetStars());
  epg.SetSeriesNumber(event.GetSeason());
  epg.SetEpisodeNumber(event.GetEpisode());
  epg.SetEpisodePartNumber(event.GetPart());
  epg.SetEpisodeName(event.GetSubtitle());
  epg.SetFlags(EPG_TAG_FLAG_UNDEFINED);
  epg.SetSeriesLink(event.GetSeriesLink());
}

void CTvheadend::TransferEvent(const Event& event, EPG_EVENT_STATE state)
{
  /* Build */
  kodi::addon::PVREPGTag tag;
  CreateEvent(event, tag);

  /* Transfer event to Kodi */
  kodi::addon::CInstancePVRClient::EpgEventStateChange(tag, state);
}

void CTvheadend::TransferEvent(kodi::addon::PVREPGTagsResultSet& results, const Event& event)
{
  /* Build */
  kodi::addon::PVREPGTag tag;
  CreateEvent(event, tag);

  /* Transfer event to Kodi */
  results.Add(tag);
}

PVR_ERROR CTvheadend::GetEPGForChannel(int channelUid,
                                       time_t start,
                                       time_t end,
                                       kodi::addon::PVREPGTagsResultSet& results)
{
  htsmsg_field_t* f;

  Logger::Log(LogLevel::LEVEL_DEBUG, "get epg channel %d start %lld stop %lld", channelUid,
              static_cast<long long>(start), static_cast<long long>(end));

  /* Build message */
  htsmsg_t* msg = htsmsg_create_map();
  htsmsg_add_u32(msg, "channelId", channelUid);
  htsmsg_add_s64(msg, "maxTime", end);

  /* Send and Wait */
  {
    std::unique_lock<std::recursive_mutex> lock(m_conn->Mutex());

    msg = m_conn->SendAndWait0(lock, "getEvents", msg);
    if (!msg)
      return PVR_ERROR_SERVER_ERROR;
  }

  /* Process */
  htsmsg_t* l = htsmsg_get_list(msg, "events");
  if (!l)
  {
    htsmsg_destroy(msg);
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed getEvents response: 'events' missing");
    return PVR_ERROR_SERVER_ERROR;
  }

  int n = 0;
  HTSMSG_FOREACH(f, l)
  {
    Event event;
    if (f->hmf_type == HMF_MAP)
    {
      if (ParseEvent(&f->hmf_msg, true, event))
      {
        /* Callback. */
        TransferEvent(results, event);
        ++n;
      }
    }
  }
  htsmsg_destroy(msg);
  Logger::Log(LogLevel::LEVEL_DEBUG, "get epg channel %d events %d", channelUid, n);

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::SetEPGMaxFutureDays(int iFutureDays)
{
  if (m_epgMaxDays != iFutureDays)
  {
    int iOldMaxDays = m_epgMaxDays;
    m_epgMaxDays = iFutureDays;

    if (m_settings->GetAsyncEpg())
    {
      Logger::Log(LogLevel::LEVEL_TRACE,
                  "reconnecting to synchronize epg data. epg max time: old = %d, new = %d",
                  iOldMaxDays, iFutureDays);
      m_conn->Disconnect(); // reconnect to synchronize epg data
    }
  }
  return PVR_ERROR_NO_ERROR;
}

/* **************************************************************************
 * Connection
 * *************************************************************************/

void CTvheadend::Disconnected()
{
}

bool CTvheadend::Connected(std::unique_lock<std::recursive_mutex>& lock)
{
  /* Query the server for available streaming profiles */
  QueryAvailableProfiles(lock);

  /* Show a notification if the profile is not available */
  const std::string streamingProfile = m_settings->GetStreamingProfile();

  if (!streamingProfile.empty() && !HasStreamingProfile(streamingProfile))
  {
    kodi::QueueFormattedNotification(QUEUE_ERROR, kodi::addon::GetLocalizedString(30502).c_str(),
                                     streamingProfile.c_str());
  }
  else
  {
    /* Tell each demuxer to use this profile from now on */
    for (auto* dmx : m_dmx)
      dmx->SetStreamingProfile(streamingProfile);
  }

  /* Request Async data, first is init (which rebuilds state) */
  if (m_asyncState.GetState() == ASYNC_NONE)
    m_asyncState.SetState(ASYNC_INIT);

  htsmsg_t* msg = htsmsg_create_map();
  if (m_settings->GetAsyncEpg())
  {
    Logger::Log(LogLevel::LEVEL_INFO, "Request async EPG (%d days)", m_epgMaxDays);
    htsmsg_add_u32(msg, "epg", 1);
    if (m_epgMaxDays > EPG_TIMEFRAME_UNLIMITED)
      htsmsg_add_s64(
          msg, "epgMaxTime",
          static_cast<int64_t>(std::time(nullptr) + m_epgMaxDays * int64_t(24 * 60 * 60)));
  }
  else
    htsmsg_add_u32(msg, "epg", 0);

  m_stateRebuilt = false;
  msg = m_conn->SendAndWait0(lock, "enableAsyncMetadata", msg);
  if (!msg)
  {
    m_asyncState.SetState(ASYNC_NONE);
    return false;
  }

  htsmsg_destroy(msg);
  Logger::Log(LogLevel::LEVEL_INFO, "Async updates requested");

  return true;
}

PVR_ERROR CTvheadend::GetBackendName(std::string& name)
{
  name = m_conn->GetServerName();
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::GetBackendVersion(std::string& version)
{
  version = m_conn->GetServerVersion();
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::GetConnectionString(std::string& connection)
{
  connection = m_conn->GetServerString();
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::GetBackendHostname(std::string& hostname)
{
  hostname = m_settings->GetConstCharHostname();
  return PVR_ERROR_NO_ERROR;
}

bool CTvheadend::HasCapability(const std::string& capability) const
{
  return m_conn->HasCapability(capability);
}

PVR_ERROR CTvheadend::OnSystemSleep()
{
  // close demuxers
  for (auto* dmx : m_dmx)
    dmx->Close();

  // close backend connection
  m_conn->OnSleep();
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CTvheadend::OnSystemWake()
{
  m_conn->OnWake();
  return PVR_ERROR_NO_ERROR;
}

/* **************************************************************************
 * VFS
 * *************************************************************************/

bool CTvheadend::OpenRecordedStream(const kodi::addon::PVRRecording& rec)
{
  if (!m_asyncState.WaitForState(ASYNC_EPG))
    return false;

  bool ret = m_vfs->Open(rec);

  if (ret)
  {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    const auto& it = m_recordings.find(std::stoul(rec.GetRecordingId()));
    if (it != m_recordings.end())
    {
      m_playingRecording = &(it->second);
    }
  }

  return ret;
}

void CTvheadend::CloseRecordedStream()
{
  m_vfs->Close();

  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  m_playingRecording = nullptr;
}

int CTvheadend::ReadRecordedStream(unsigned char* buf, unsigned int len)
{
  return m_vfs->Read(buf, len, VfsIsActiveRecording());
}

int64_t CTvheadend::SeekRecordedStream(int64_t position, int whence)
{
  return m_vfs->Seek(position, whence, VfsIsActiveRecording());
}

int64_t CTvheadend::LengthRecordedStream()
{
  return m_vfs->Size();
}

void CTvheadend::PauseStream(bool paused)
{
  if (VfsIsActiveRecording())
    m_vfs->PauseStream(paused);
}

PVR_ERROR CTvheadend::GetStreamReadChunkSize(int& chunksize)
{
  if (!chunksize)
    return PVR_ERROR_INVALID_PARAMETERS;

  chunksize = m_settings->GetStreamReadChunkSize() * 1024;
  return PVR_ERROR_NO_ERROR;
}

bool CTvheadend::IsRealTimeStream()
{
  if (m_playingRecording)
    return m_vfs->IsRealTimeStream();
  else
    return m_dmx_active->IsRealTimeStream();
}

/* **************************************************************************
 * IHTSPDemuxPacketHandler implementation
 * *************************************************************************/

kodi::addon::PVRCodec CTvheadend::GetCodecByName(const std::string& codecName) const
{
  return kodi::addon::CInstancePVRClient::GetCodecByName(codecName);
}

DEMUX_PACKET* CTvheadend::AllocateDemuxPacket(int iDataSize)
{
  return kodi::addon::CInstancePVRClient::AllocateDemuxPacket(iDataSize);
}

void CTvheadend::FreeDemuxPacket(DEMUX_PACKET* pPacket)
{
  kodi::addon::CInstancePVRClient::FreeDemuxPacket(pPacket);
}

/* **************************************************************************
 * Message handling
 * *************************************************************************/

bool CTvheadend::ProcessMessage(const std::string& method, htsmsg_t* msg)
{
  uint32_t subId = 0;
  if (!htsmsg_get_u32(msg, "subscriptionId", &subId))
  {
    /* subscriptionId found - for a Demuxer */
    for (auto* dmx : m_dmx)
    {
      if (dmx->GetSubscriptionId() == subId)
        return dmx->ProcessMessage(method, msg);
    }
    return true;
  }

  /* Store */
  m_queue.Push(HTSPMessage(method, msg));
  return false;
}

void CTvheadend::ConnectionStateChange(const std::string& connectionString,
                                       PVR_CONNECTION_STATE newState,
                                       const std::string& message)
{
  kodi::addon::CInstancePVRClient::ConnectionStateChange(connectionString, newState, message);
}

void CTvheadend::CloseExpiredSubscriptions()
{
  // predictive tuning active?
  if (m_dmx.size() > 1)
  {
    int closeDelay = m_settings->GetPreTunerCloseDelay();
    if (closeDelay > 0)
    {
      for (auto* dmx : m_dmx)
      {
        // do not close the running subscription if it is currently paused
        if (m_playingLiveStream && dmx == m_dmx_active && dmx->IsPaused())
          continue;

        time_t lastUse = dmx->GetLastUse();
        if (lastUse > 0 && lastUse + closeDelay < std::time(nullptr))
        {
          Logger::Log(LogLevel::LEVEL_TRACE, "closing expired subscription %u",
                      dmx->GetSubscriptionId());
          dmx->Close();
        }
      }
    }
  }
}

void CTvheadend::Process()
{
  while (!m_threadStop)
  {
    /* Check Q */
    // this is a bit horrible, but meh
    HTSPMessage msg = {};
    bool bSuccess = m_queue.Pop(msg, 2000);

    if (m_threadStop)
      continue;

    // check for expired predictive tuning subscriptions and close those
    CloseExpiredSubscriptions();

    if (!bSuccess || !msg.GetHTSPMessage())
      continue;

    const std::string& method = msg.GetMethod();

    SHTSPEventList eventsCopy;
    /* Scope lock for processing */
    {
      std::lock_guard<std::recursive_mutex> lock(m_mutex);

      /* Channels */
      if (method == "channelAdd")
        ParseChannelAddOrUpdate(msg.GetHTSPMessage(), true);
      else if (method == "channelUpdate")
        ParseChannelAddOrUpdate(msg.GetHTSPMessage(), false);
      else if (method == "channelDelete")
        ParseChannelDelete(msg.GetHTSPMessage());

      /* Channel Tags (aka channel groups)*/
      else if (method == "tagAdd")
        ParseTagAddOrUpdate(msg.GetHTSPMessage(), true);
      else if (method == "tagUpdate")
        ParseTagAddOrUpdate(msg.GetHTSPMessage(), false);
      else if (method == "tagDelete")
        ParseTagDelete(msg.GetHTSPMessage());

      /* Recordings */
      else if (method == "dvrEntryAdd")
        ParseRecordingAddOrUpdate(msg.GetHTSPMessage(), true);
      else if (method == "dvrEntryUpdate")
        ParseRecordingAddOrUpdate(msg.GetHTSPMessage(), false);
      else if (method == "dvrEntryDelete")
        ParseRecordingDelete(msg.GetHTSPMessage());

      /* Timerec */
      else if (method == "timerecEntryAdd")
      {
        if (m_timeRecordings.ParseTimerecAddOrUpdate(msg.GetHTSPMessage(), true))
          TriggerTimerUpdate();
      }
      else if (method == "timerecEntryUpdate")
      {
        if (m_timeRecordings.ParseTimerecAddOrUpdate(msg.GetHTSPMessage(), false))
          TriggerTimerUpdate();
      }
      else if (method == "timerecEntryDelete")
      {
        if (m_timeRecordings.ParseTimerecDelete(msg.GetHTSPMessage()))
          TriggerTimerUpdate();
      }

      /* Autorec */
      else if (method == "autorecEntryAdd")
      {
        if (m_autoRecordings.ParseAutorecAddOrUpdate(msg.GetHTSPMessage(), true))
          TriggerTimerUpdate();
      }
      else if (method == "autorecEntryUpdate")
      {
        if (m_autoRecordings.ParseAutorecAddOrUpdate(msg.GetHTSPMessage(), false))
          TriggerTimerUpdate();
      }
      else if (method == "autorecEntryDelete")
      {
        if (m_autoRecordings.ParseAutorecDelete(msg.GetHTSPMessage()))
          TriggerTimerUpdate();
      }

      /* EPG */
      else if (method == "eventAdd")
        ParseEventAddOrUpdate(msg.GetHTSPMessage(), true);
      else if (method == "eventUpdate")
        ParseEventAddOrUpdate(msg.GetHTSPMessage(), false);
      else if (method == "eventDelete")
        ParseEventDelete(msg.GetHTSPMessage());

      /* ASync complete */
      else if (method == "initialSyncCompleted")
        SyncCompleted();

      /* Unknown */
      else
        Logger::Log(LogLevel::LEVEL_DEBUG, "unhandled message [%s]", method.c_str());

      /* make a copy of events list to process it without lock. */
      eventsCopy = m_events;
      m_events.clear();
    }

    /* Manual delete rather than waiting */
    msg.ClearMessage();

    if (m_threadStop)
      continue;

    /* Process events
     * Note: due to potential deadly embrace this must be done without the
     *       m_mutex held!
     */
    for (const auto& event : eventsCopy)
    {
      switch (event.m_type)
      {
        case HTSP_EVENT_TAG_UPDATE:
          kodi::addon::CInstancePVRClient::TriggerChannelGroupsUpdate();
          break;
        case HTSP_EVENT_CHN_UPDATE:
          kodi::addon::CInstancePVRClient::TriggerChannelUpdate();
          break;
        case HTSP_EVENT_REC_UPDATE:
          kodi::addon::CInstancePVRClient::TriggerTimerUpdate();
          kodi::addon::CInstancePVRClient::TriggerRecordingUpdate();
          break;
        case HTSP_EVENT_EPG_UPDATE:
          TransferEvent(event.m_epg, event.m_state);
          break;
        case HTSP_EVENT_NONE:
          break;
      }
    }
  }
}

void CTvheadend::TriggerChannelGroupsUpdate()
{
  m_events.emplace_back(SHTSPEvent(HTSP_EVENT_TAG_UPDATE));
}

void CTvheadend::TriggerChannelUpdate()
{
  m_events.emplace_back(SHTSPEvent(HTSP_EVENT_CHN_UPDATE));
}

void CTvheadend::TriggerRecordingUpdate()
{
  m_events.emplace_back(SHTSPEvent(HTSP_EVENT_REC_UPDATE));
}

void CTvheadend::TriggerTimerUpdate()
{
  m_events.emplace_back(SHTSPEvent(HTSP_EVENT_REC_UPDATE));
}

void CTvheadend::PushEpgEventUpdate(const Event& epg, EPG_EVENT_STATE state)
{
  SHTSPEvent event = SHTSPEvent(HTSP_EVENT_EPG_UPDATE, epg, state);

  if (std::find(m_events.begin(), m_events.end(), event) == m_events.end())
    m_events.emplace_back(event);
}

void CTvheadend::SyncInitCompleted()
{
  if (!m_stateRebuilt)
  {
    m_stateRebuilt = true;

    for (auto* dmx : m_dmx)
      dmx->RebuildState();

    m_vfs->RebuildState();
  }

  /* check state engine */
  if (m_asyncState.GetState() != ASYNC_INIT)
    return;

  /* Rebuild state */
  m_timeRecordings.RebuildState();
  m_autoRecordings.RebuildState();

  /* Flag all async fields in case they've been deleted */
  for (auto& entry : m_channels)
    entry.second.SetDirty(true);
  for (auto& entry : m_tags)
    entry.second.SetDirty(true);
  for (auto& entry : m_schedules)
    entry.second.SetDirty(true);
  for (auto& entry : m_recordings)
    entry.second.SetDirty(true);

  /* Next */
  m_asyncState.SetState(ASYNC_CHN);
}

void CTvheadend::SyncChannelsCompleted()
{
  SyncInitCompleted();

  /* check state engine */
  if (m_asyncState.GetState() != ASYNC_CHN)
    return;

  /* Tags */
  utilities::erase_if(m_tags, [](const TagMapEntry& entry) { return entry.second.IsDirty(); });

  TriggerChannelGroupsUpdate();

  /* Channels */
  utilities::erase_if(m_channels,
                      [](const ChannelMapEntry& entry) { return entry.second.IsDirty(); });

  TriggerChannelUpdate();

  /* Next */
  m_asyncState.SetState(ASYNC_DVR);
}

void CTvheadend::SyncDvrCompleted()
{
  SyncChannelsCompleted();

  /* check state engine */
  if (m_asyncState.GetState() != ASYNC_DVR)
    return;

  /* Recordings */
  {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // save id of currently playing recording, if any
    uint32_t id = m_playingRecording ? m_playingRecording->GetId() : 0;

    utilities::erase_if(m_recordings,
                        [](const RecordingMapEntry& entry) { return entry.second.IsDirty(); });

    if (m_playingRecording)
    {
      const auto& it = m_recordings.find(id);
      if (it == m_recordings.end())
        m_playingRecording = nullptr;
    }
  }

  /* Time-based repeating timers */
  m_timeRecordings.SyncDvrCompleted();

  /* EPG-query-based repeating timers */
  m_autoRecordings.SyncDvrCompleted();

  TriggerRecordingUpdate();
  TriggerTimerUpdate();

  /* Next */
  m_asyncState.SetState(ASYNC_EPG);
}

void CTvheadend::SyncEpgCompleted()
{
  SyncDvrCompleted();

  /* check state engine */
  if (m_asyncState.GetState() != ASYNC_EPG)
    return;

  if (!m_settings->GetAsyncEpg())
  {
    m_asyncState.SetState(ASYNC_DONE);
    return;
  }

  /* Schedules */
  std::vector<std::pair<uint32_t, uint32_t>> deletedEvents;
  utilities::erase_if(m_schedules, [&](const ScheduleMapEntry& entry) {
    if (entry.second.IsDirty())
    {
      // all events are dirty too!
      for (auto& evt : entry.second.GetEvents())
        deletedEvents.emplace_back(std::make_pair(evt.second.GetId() /* event uid */,
                                                  entry.second.GetId() /* channel uid */));
      return true;
    }
    return false;
  });

  /* Events */
  for (auto& entry : m_schedules)
  {
    utilities::erase_if(entry.second.GetEvents(), [&](const EventUidsMapEntry& mapEntry) {
      if (mapEntry.second.IsDirty())
      {
        deletedEvents.emplace_back(std::make_pair(mapEntry.second.GetId() /* event uid */,
                                                  entry.second.GetId() /* channel uid */));
        return true;
      }
      return false;
    });
  }

  for (auto& entry : deletedEvents)
  {
    /* Transfer event to Kodi (callback) */
    Event evt;
    evt.SetId(entry.first);
    evt.SetChannel(entry.second);
    PushEpgEventUpdate(evt, EPG_EVENT_DELETED);
  }

  /* Next */
  m_asyncState.SetState(ASYNC_DONE);
}

void CTvheadend::SyncCompleted()
{
  SyncEpgCompleted();

  m_asyncState.SetState(ASYNC_DONE);

  Logger::Log(LogLevel::LEVEL_INFO, "Async updates initialised");
}

void CTvheadend::ParseTagAddOrUpdate(htsmsg_t* msg, bool bAdd)
{
  /* Rebuild state upon arrival of first async data */
  SyncInitCompleted();

  /* Validate */
  uint32_t u32 = 0;
  if (htsmsg_get_u32(msg, "tagId", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed tagAdd/tagUpdate: 'tagId' missing");
    return;
  }

  /* Locate object */
  if (bAdd && m_tags.find(u32) != m_tags.cend())
  {
    Logger::Log(LogLevel::LEVEL_DEBUG, "Ignoring 'addTag' for existing tag with id %d", u32);
    return;
  }

  auto& existingTag = m_tags[u32];
  existingTag.SetDirty(false);

  /* Create new object */
  Tag tag;
  tag.SetId(u32);

  /* Index */
  if (!htsmsg_get_u32(msg, "tagIndex", &u32))
    tag.SetIndex(u32);

  /* Name */
  const char* str = htsmsg_get_str(msg, "tagName");
  if (str)
  {
    tag.SetName(str);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed tagAdd: 'tagName' missing");
    return;
  }

  /* Icon */
  str = htsmsg_get_str(msg, "tagIcon");
  if (str)
    tag.SetIcon(GetImageURL(str));

  /* Members */
  htsmsg_t* list = htsmsg_get_list(msg, "members");
  if (list)
  {
    htsmsg_field_t* f;
    HTSMSG_FOREACH(f, list)
    {
      if (f->hmf_type != HMF_S64)
        continue;
      tag.GetChannels().emplace_back(static_cast<int>(f->hmf_s64));
    }
  }

  /* Update */
  if (existingTag != tag)
  {
    existingTag = tag;

    Logger::Log(LogLevel::LEVEL_DEBUG, "tag updated id:%u, name:%s", existingTag.GetId(),
                existingTag.GetName().c_str());
    if (m_asyncState.GetState() > ASYNC_CHN)
      TriggerChannelGroupsUpdate();
  }
}

void CTvheadend::ParseTagDelete(htsmsg_t* msg)
{
  /* Validate */
  uint32_t u32 = 0;
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

void CTvheadend::ParseChannelAddOrUpdate(htsmsg_t* msg, bool bAdd)
{
  /* Rebuild state upon arrival of first async data */
  SyncInitCompleted();

  /* Validate */
  uint32_t u32 = 0;
  if (htsmsg_get_u32(msg, "channelId", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed channelAdd/channelUpdate: 'channelId' missing");
    return;
  }

  /* Locate channel object */
  Channel& channel = m_channels[u32];
  Channel comparison = channel;
  channel.SetId(u32);
  channel.SetDirty(false);

  /* Channel name */
  const char* str = htsmsg_get_str(msg, "channelName");
  if (str)
  {
    channel.SetName(str);
  }
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
  str = htsmsg_get_str(msg, "channelIcon");
  if (str)
    channel.SetIcon(GetImageURL(str));

  /* Services */
  htsmsg_t* list = htsmsg_get_list(msg, "services");
  if (list)
  {
    htsmsg_field_t* f = nullptr;
    uint32_t caid = 0;
    HTSMSG_FOREACH(f, list)
    {
      if (f->hmf_type != HMF_MAP)
        continue;

      /* Channel type */
      if (!htsmsg_get_u32(&f->hmf_msg, "content", &u32))
        channel.SetType(u32);

      /* CAID */
      if (caid == 0)
        htsmsg_get_u32(&f->hmf_msg, "caid", &caid);
    }

    channel.SetCaid(caid);
  }

  /* Update Kodi */
  if (channel != comparison)
  {
    Logger::Log(LogLevel::LEVEL_DEBUG, "channel %s id:%u, name:%s", (bAdd ? "added" : "updated"),
                channel.GetId(), channel.GetName().c_str());

    if (bAdd)
      m_channelTuningPredictor.AddChannel(channel);
    else
      m_channelTuningPredictor.UpdateChannel(comparison, channel);

    if (m_asyncState.GetState() > ASYNC_CHN)
      TriggerChannelUpdate();
  }
}

void CTvheadend::ParseChannelDelete(htsmsg_t* msg)
{
  /* Validate */
  uint32_t u32 = 0;
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

void CTvheadend::ParseRecordingAddOrUpdate(htsmsg_t* msg, bool bAdd)
{
  /* Channels complete */
  SyncChannelsCompleted();

  /* Validate */
  uint32_t id = 0;
  if (htsmsg_get_u32(msg, "id", &id))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd/dvrEntryUpdate: 'id' missing");
    return;
  }

  /* Ignore duplicates */
  uint32_t dup = 0;
  if (m_settings->GetIgnoreDuplicateSchedules() && !htsmsg_get_u32(msg, "duplicate", &dup) &&
      dup == 1)
    return;

  /* Ignore recordings without a file (e.g. removed recordings) */
  const char* error = htsmsg_get_str(msg, "error");
  if (error && (strstr(error, "missing") != nullptr))
  {
    const auto it = m_recordings.find(id);
    if (it != m_recordings.end())
    {
      m_recordings.erase(it);

      if (m_asyncState.GetState() > ASYNC_DVR)
      {
        TriggerTimerUpdate();
        TriggerRecordingUpdate();
      }
    }
    return;
  }

  /* Get/create entry */
  Recording& rec = m_recordings[id];
  Recording comparison = rec;
  rec.SetId(id);
  rec.SetDirty(false);

  {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_playingRecording && m_playingRecording->GetId() == id)
      m_playingRecording = &rec;
  }

  // Set the time the recording was scheduled to start. This may differ from the actual start.
  int64_t start = 0;
  if (!htsmsg_get_s64(msg, "start", &start))
    rec.SetStart(start);
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd: 'start' missing");
    return;
  }

  // Set the time the recording was scheduled to stop. This may differ from the actual stop.
  int64_t stop = 0;
  if (!htsmsg_get_s64(msg, "stop", &stop))
    rec.SetStop(stop);
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd: 'stop' missing");
    return;
  }

  /* Channel is optional, it may not exist anymore */
  uint32_t channel = 0;
  if (!htsmsg_get_u32(msg, "channel", &channel))
  {
    /* Channel Id */
    rec.SetChannel(channel);

    auto cit = m_channels.find(rec.GetChannel());
    if (cit != m_channels.cend())
    {
      /* Channel type */
      rec.SetChannelType(cit->second.GetType());

      /* Channel name */
      rec.SetChannelName(cit->second.GetName());
    }
  }

  htsmsg_t* files = htsmsg_get_list(msg, "files");
  if (files)
  {
    bool needChannelType = !rec.GetChannelType();
    bool hasAudio = false;
    bool hasVideo = false;

    // For multi-file recordings, tvheadend will always play the one recorded last.
    // No way (using HTSP) known to control which file to play.
    // So, collect meta data for latest file of the multi-file recording.
    start = 0;
    htsmsg_t* lastfile = nullptr;

    htsmsg_field_t* file = nullptr;
    HTSMSG_FOREACH(file, files) // Loop through all files
    {
      if (file->hmf_type != HMF_MAP)
        continue;

      int64_t s64 = 0;
      if (!htsmsg_get_s64(&file->hmf_msg, "start", &s64) && (start == 0 || start < s64))
      {
        start = s64;
        lastfile = &file->hmf_msg;
      }
    }

    if (lastfile)
    {
      if (needChannelType && !(hasAudio && hasVideo))
      {
        htsmsg_t* streams = htsmsg_get_list(lastfile, "info");
        if (streams)
        {
          htsmsg_field_t* stream = nullptr;
          HTSMSG_FOREACH(stream, streams) // Loop through all streams
          {
            if (stream->hmf_type != HMF_MAP)
              continue;

            uint32_t u32 = 0;
            if (!htsmsg_get_u32(&stream->hmf_msg, "audio_type",
                                &u32)) // Only present for audio streams
              hasAudio = true;

            if (!htsmsg_get_u32(&stream->hmf_msg, "aspect_num",
                                &u32)) // Only present for video streams
              hasVideo = true;
          }
        }
      }

      int64_t s64 = 0;
      if (!htsmsg_get_s64(lastfile, "start", &s64))
        rec.SetFilesStart(s64);

      if (!htsmsg_get_s64(lastfile, "stop", &s64))
        rec.SetFilesStop(s64);

      if (!htsmsg_get_s64(lastfile, "size", &s64))
        rec.SetFilesSize(s64);
    }

    /* Channel type fallback (in case channel was deleted) */
    if (needChannelType)
      rec.SetChannelType(hasVideo ? CHANNEL_TYPE_TV
                                  : (hasAudio ? CHANNEL_TYPE_RADIO : CHANNEL_TYPE_OTHER));
  }

  /* Channel name fallback (in case channel was deleted) */
  if (rec.GetChannelName().empty())
  {
    const char* str = htsmsg_get_str(msg, "channelName");
    if (str)
      rec.SetChannelName(str);
  }

  int64_t startExtra = 0;
  if (!htsmsg_get_s64(msg, "startExtra", &startExtra))
    rec.SetStartExtra(startExtra);
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd: 'startExtra' missing");
    return;
  }

  int64_t stopExtra = 0;
  if (!htsmsg_get_s64(msg, "stopExtra", &stopExtra))
    rec.SetStopExtra(stopExtra);
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd: 'stopExtra' missing");
    return;
  }

  uint32_t removal = 0;
  if (!htsmsg_get_u32(msg, "removal", &removal))
  {
    rec.SetLifetime(removal);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd: 'removal' missing");
    return;
  }

  uint32_t priority = 0;
  if (!htsmsg_get_u32(msg, "priority", &priority))
  {
    switch (priority)
    {
      case DVR_PRIO_IMPORTANT:
      case DVR_PRIO_HIGH:
      case DVR_PRIO_NORMAL:
      case DVR_PRIO_LOW:
      case DVR_PRIO_UNIMPORTANT:
      case DVR_PRIO_DEFAULT:
        rec.SetPriority(priority);
        break;
      default:
        Logger::Log(LogLevel::LEVEL_ERROR,
                    "malformed dvrEntryAdd/dvrEntryUpdate: unknown priority value %d", priority);
        return;
    }
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd: 'priority' missing");
    return;
  }

  /* Parse state */
  const char* state = htsmsg_get_str(msg, "state");
  if (state)
  {
    if (strstr(state, "scheduled"))
      rec.SetState(PVR_TIMER_STATE_SCHEDULED);
    else if (strstr(state, "recording"))
      rec.SetState(PVR_TIMER_STATE_RECORDING);
    else if (strstr(state, "completed"))
      rec.SetState(PVR_TIMER_STATE_COMPLETED);
    else if (strstr(state, "missed"))
      rec.SetState(PVR_TIMER_STATE_ERROR);
    else if (strstr(state, "invalid"))
      rec.SetState(PVR_TIMER_STATE_ERROR);
  }
  else if (bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryAdd: 'state' missing");
    return;
  }

  /* Add optional fields */
  uint32_t eventId = 0;
  if (!htsmsg_get_u32(msg, "eventId", &eventId))
    rec.SetEventId(eventId);

  uint32_t enabled = 0;
  if (!htsmsg_get_u32(msg, "enabled", &enabled))
    rec.SetEnabled(enabled);

  const char* str = htsmsg_get_str(msg, "title");
  if (str)
    rec.SetTitle(str);

  str = htsmsg_get_str(msg, "subtitle");
  if (str)
    rec.SetSubtitle(str);

  str = htsmsg_get_str(msg, "path");
  if (str)
    rec.SetPath(str);

  str = htsmsg_get_str(msg, "description");
  if (str)
  {
    rec.SetDescription(str);
  }
  else
  {
    str = htsmsg_get_str(msg, "summary");
    if (str)
      rec.SetDescription(str);
  }

  uint32_t contentType = 0;
  if (!htsmsg_get_u32(msg, "contentType", &contentType))
    rec.SetContentType(contentType);

  str = htsmsg_get_str(msg, "timerecId");
  if (str)
    rec.SetTimerecId(str);

  str = htsmsg_get_str(msg, "autorecId");
  if (str)
    rec.SetAutorecId(str);

  str = htsmsg_get_str(msg, "image");
  if (str)
    rec.SetImage(GetImageURL(str));

  str = htsmsg_get_str(msg, "fanartImage");
  if (str)
    rec.SetFanartImage(GetImageURL(str));

  if (m_conn->GetProtocol() >= 32)
  {
    if (rec.GetDescription().empty() && !rec.GetSubtitle().empty())
    {
      /*
        Due to changes in HTSP v32, if the description is empty, try
        to use the subtitle as the description. Clear the subtitle
        afterwards to avoid duplicate information being displayed.

        This was done by TVHeadend prior to HTSP v32.
      */
      rec.SetDescription(rec.GetSubtitle());
      rec.SetSubtitle("");
    }
  }

  /* Error */
  if (error)
  {
    if (!std::strcmp(error, "300"))
      rec.SetState(PVR_TIMER_STATE_ABORTED);

    rec.SetError(error);
  }

  /* A running recording will have an active subscription assigned to it */
  if (rec.GetState() == PVR_TIMER_STATE_RECORDING)
  {
    /* Parse subscription error */
    /* This field is absent when everything is fine or when htsp version < 20 */
    str = htsmsg_get_str(msg, "subscriptionError");
    if (str)
    {
      /* No free adapter, AKA subscription conflict */
      if (!std::strcmp("noFreeAdapter", str))
        rec.SetState(PVR_TIMER_STATE_CONFLICT_NOK);
    }
  }

  /* Play status (optional) */
  if (m_conn->GetProtocol() >= 27)
  {
    uint32_t playCount = 0;
    if (!htsmsg_get_u32(msg, "playcount", &playCount))
      rec.SetPlayCount(playCount);

    uint32_t playPosition = 0;
    if (!htsmsg_get_u32(msg, "playposition", &playPosition))
      rec.SetPlayPosition(playPosition);
  }

  /* season/episode/part */
  uint32_t season = 0;
  if (!htsmsg_get_u32(msg, "seasonNumber", &season))
    rec.SetSeason(static_cast<int32_t>(season));

  uint32_t episode = 0;
  if (!htsmsg_get_u32(msg, "episodeNumber", &episode))
    rec.SetEpisode(static_cast<int32_t>(episode));

  uint32_t part = 0;
  if (!htsmsg_get_u32(msg, "partNumber", &part))
    rec.SetPart(static_cast<int32_t>(part));

  /* Update */
  if (rec != comparison)
  {
    const std::string error = rec.GetError().empty() ? "n/a" : rec.GetError();

    Logger::Log(LogLevel::LEVEL_DEBUG, "recording id:%d, state:%s, title:%s, error:%s",
                rec.GetId(), state, rec.GetTitle().c_str(), error.c_str());

    if (m_asyncState.GetState() > ASYNC_DVR)
    {
      TriggerTimerUpdate();
      TriggerRecordingUpdate();
    }
  }
}

void CTvheadend::ParseRecordingDelete(htsmsg_t* msg)
{
  /* Validate */
  uint32_t u32 = 0;
  if (htsmsg_get_u32(msg, "id", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed dvrEntryDelete: 'id' missing");
    return;
  }
  Logger::Log(LogLevel::LEVEL_DEBUG, "delete recording %u", u32);

  /* Erase */
  {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_playingRecording && m_playingRecording->GetId() == u32)
      m_playingRecording = nullptr;

    m_recordings.erase(u32);
  }

  /* Update */
  TriggerTimerUpdate();
  TriggerRecordingUpdate();
}

bool CTvheadend::ParseEvent(htsmsg_t* msg, bool bAdd, Event& evt)
{
  /* Recordings complete */
  SyncDvrCompleted();

  /* Validate */
  uint32_t id = 0;
  if (htsmsg_get_u32(msg, "eventId", &id))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed eventAdd/eventUpdate: 'eventId' missing");
    return false;
  }

  uint32_t channel = 0;
  if (htsmsg_get_u32(msg, "channelId", &channel) && bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed eventAdd: 'channelId' missing");
    return false;
  }

  int64_t start = 0;
  if (htsmsg_get_s64(msg, "start", &start) && bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed eventAdd: 'start' missing");
    return false;
  }

  int64_t stop = 0;
  if (htsmsg_get_s64(msg, "stop", &stop) && bAdd)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed eventAdd: 'stop' missing");
    return false;
  }

  evt.SetId(id);
  evt.SetChannel(channel);
  evt.SetStart(static_cast<time_t>(start));
  evt.SetStop(static_cast<time_t>(stop));

  /* Add optional fields */
  const char* str = htsmsg_get_str(msg, "title");
  if (str)
    evt.SetTitle(str);

  str = htsmsg_get_str(msg, "subtitle");
  if (str)
    evt.SetSubtitle(str);

  str = htsmsg_get_str(msg, "summary");
  if (str)
    evt.SetSummary(str);

  str = htsmsg_get_str(msg, "description");
  if (str)
    evt.SetDesc(str);

  str = htsmsg_get_str(msg, "image");
  if (str)
    evt.SetImage(GetImageURL(str));

  uint32_t u32 = 0;
  if (!htsmsg_get_u32(msg, "nextEventId", &u32))
    evt.SetNext(u32);
  if (!htsmsg_get_u32(msg, "contentType", &u32))
    evt.SetContent(u32);
  if (!htsmsg_get_u32(msg, "starRating", &u32))
    evt.SetStars(u32);
  if (!htsmsg_get_u32(msg, "ageRating", &u32))
    evt.SetAge(u32);

  int64_t s64 = 0;
  if (!htsmsg_get_s64(msg, "firstAired", &s64))
    evt.SetAired(static_cast<time_t>(s64));
  if (!htsmsg_get_u32(msg, "seasonNumber", &u32))
    evt.SetSeason(static_cast<int32_t>(u32));
  if (!htsmsg_get_u32(msg, "episodeNumber", &u32))
    evt.SetEpisode(static_cast<int32_t>(u32));
  if (!htsmsg_get_u32(msg, "partNumber", &u32))
    evt.SetPart(u32);

  str = htsmsg_get_str(msg, "serieslinkUri");
  if (str)
    evt.SetSeriesLink(str);

  if (!htsmsg_get_u32(msg, "copyrightYear", &u32))
    evt.SetYear(u32);
  if (!htsmsg_get_u32(msg, "dvrId", &u32))
    evt.SetRecordingId(u32);

  if (m_conn->GetProtocol() >= 32)
  {
    if (evt.GetDesc().empty())
    {
      /*
        Due to changes in HTSP v32, if the description is empty, try
        to use the summary as the description. If the summary is empty,
        try to use the subtitle as the description. Also clear the
        respective entries to avoid duplicate information being displayed.

        This was done by TVHeadend prior to HTSP v32.
      */
      if (!evt.GetSummary().empty())
      {
        evt.SetDesc(evt.GetSummary());
        evt.SetSummary("");
      }
      else if (!evt.GetSubtitle().empty())
      {
        evt.SetDesc(evt.GetSubtitle());
        evt.SetSubtitle("");
      }
    }
  }

  htsmsg_t* l = htsmsg_get_map(msg, "credits");
  if (l)
  {
    std::vector<std::string> writers;
    std::vector<std::string> directors;
    std::vector<std::string> cast;

    htsmsg_field_t* f = nullptr;
    HTSMSG_FOREACH(f, l)
    {
      if (!f->hmf_name)
        continue;

      const char* str = htsmsg_field_get_string(f);
      if (!str)
        continue;

      if (!std::strcmp(str, "writer"))
        writers.emplace_back(f->hmf_name);
      else if (!std::strcmp(str, "director"))
        directors.emplace_back(f->hmf_name);
      else if (!std::strcmp(str, "actor") || !std::strcmp(str, "guest") ||
               !std::strcmp(str, "presenter"))
        cast.emplace_back(f->hmf_name);
    }

    evt.SetWriters(writers);
    evt.SetDirectors(directors);
    evt.SetCast(cast);
  }

  l = htsmsg_get_list(msg, "category");
  if (l)
  {
    std::vector<std::string> categories;

    htsmsg_field_t* f = nullptr;
    HTSMSG_FOREACH(f, l)
    {
      const char* str = f->hmf_str;
      if (str != nullptr)
        categories.emplace_back(str);
    }

    evt.SetCategories(categories);
  }

  return true;
}

void CTvheadend::ParseEventAddOrUpdate(htsmsg_t* msg, bool bAdd)
{
  Event evt;

  /* Parse */
  if (!ParseEvent(msg, bAdd, evt))
    return;

  /* create/update schedule */
  Schedule& sched = m_schedules[evt.GetChannel()];
  sched.SetId(evt.GetChannel());
  sched.SetDirty(false);

  /* create/update event */
  EventUids& events = sched.GetEvents();

  bool bUpdated = false;
  if (bAdd && m_asyncState.GetState() == ASYNC_DONE)
  {
    // After a reconnect, during processing of "enableAsyncMetadata" htsp
    // method, tvheadend sends all events as "added". Check whether we
    // announced the event already and in case send it as "updated" to Kodi.
    auto it = events.find(evt.GetId());
    if (it != events.end())
    {
      bUpdated = true;

      Entity& ent = it->second;
      ent.SetId(evt.GetId());
      ent.SetDirty(false);
    }
  }

  if (!bUpdated)
  {
    Entity& ent = events[evt.GetId()];
    ent.SetId(evt.GetId());
    ent.SetDirty(false);
  }

  Logger::Log(LogLevel::LEVEL_TRACE, "event id:%d channel:%d start:%d stop:%d title:%s desc:%s",
              evt.GetId(), evt.GetChannel(), static_cast<int>(evt.GetStart()),
              static_cast<int>(evt.GetStop()), evt.GetTitle().c_str(), evt.GetDesc().c_str());

  /* Transfer event to Kodi (callback) */
  PushEpgEventUpdate(evt, (!bAdd || bUpdated) ? EPG_EVENT_UPDATED : EPG_EVENT_CREATED);
}

void CTvheadend::ParseEventDelete(htsmsg_t* msg)
{
  /* Validate */
  uint32_t u32 = 0;
  if (htsmsg_get_u32(msg, "eventId", &u32))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed eventDelete: 'eventId' missing");
    return;
  }
  Logger::Log(LogLevel::LEVEL_TRACE, "delete event %u", u32);

  /* Erase */
  for (auto& entry : m_schedules)
  {
    Schedule& schedule = entry.second;
    EventUids& events = schedule.GetEvents();

    // Find the event so we can get the channel number
    auto eit = events.find(u32);

    if (eit != events.end())
    {
      Logger::Log(LogLevel::LEVEL_TRACE, "deleted event %d from channel %d", u32, schedule.GetId());
      events.erase(eit);

      /* Transfer event to Kodi (callback) */
      Event evt;
      evt.SetId(u32);
      evt.SetChannel(schedule.GetId());
      PushEpgEventUpdate(evt, EPG_EVENT_DELETED);
      return;
    }
  }
}

uint32_t CTvheadend::GetNextUnnumberedChannelNumber()
{
  static uint32_t number = UNNUMBERED_CHANNEL;
  return number++;
}

void CTvheadend::TuneOnOldest(uint32_t channelId)
{
  HTSPDemuxer* oldest = nullptr;

  for (auto* dmx : m_dmx)
  {
    if (dmx->GetChannelId() == channelId)
    {
      dmx->Weight(SUBSCRIPTION_WEIGHT_PRETUNING);
      return;
    }
    if (dmx == m_dmx_active)
      continue;

    if (!oldest || dmx->GetLastUse() <= oldest->GetLastUse())
      oldest = dmx;
  }

  if (oldest)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "pretuning channel %u on subscription %u",
                m_channels[channelId].GetNum(), oldest->GetSubscriptionId());
    oldest->Open(channelId, SUBSCRIPTION_WEIGHT_PRETUNING);
  }
}

void CTvheadend::PredictiveTune(uint32_t fromChannelId, uint32_t toChannelId)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  /* Consult the predictive tuning helper for which channel
   * should be predictably tuned next */
  uint32_t predictedChannelId =
      m_channelTuningPredictor.PredictNextChannelId(fromChannelId, toChannelId);

  if (predictedChannelId != predictivetune::CHANNEL_ID_NONE)
    TuneOnOldest(predictedChannelId);
}

bool CTvheadend::OpenLiveStream(const kodi::addon::PVRChannel& chn)
{
  HTSPDemuxer* oldest = m_dmx[0];

  if (m_dmx.size() == 1)
  {
    /* speedup things if we don't use predictive tuning */
    m_playingLiveStream = oldest->Open(chn.GetUniqueId(), SUBSCRIPTION_WEIGHT_SERVERCONF);
    m_dmx_active = oldest;
    return m_playingLiveStream;
  }

  /* If we have a lingering subscription for the target channel
   * we reuse that subscription */
  for (auto* dmx : m_dmx)
  {
    if (dmx->GetChannelId() == chn.GetUniqueId())
    {
      Logger::Log(LogLevel::LEVEL_TRACE, "retuning channel %u on subscription %u",
                  m_channels[chn.GetUniqueId()].GetNum(), dmx->GetSubscriptionId());

      if (dmx != m_dmx_active)
      {
        /* Lower the priority on the current subscription */
        m_dmx_active->Weight(SUBSCRIPTION_WEIGHT_POSTTUNING);
        uint32_t prevId = m_dmx_active->GetChannelId();

        /* Promote the lingering subscription to the active one */
        dmx->Weight(SUBSCRIPTION_WEIGHT_NORMAL);
        m_dmx_active = dmx;

        PredictiveTune(prevId, chn.GetUniqueId());
        m_streamchange = true;
      }

      m_playingLiveStream = true;
      return true;
    }

    if (dmx->GetLastUse() < oldest->GetLastUse())
      oldest = dmx;
  }

  /* If we don't have an existing subscription for the channel we create one
   * on the oldest demuxer */
  Logger::Log(LogLevel::LEVEL_TRACE, "tuning channel %u on subscription %u",
              m_channels[chn.GetUniqueId()].GetNum(), oldest->GetSubscriptionId());

  uint32_t prevId = m_dmx_active->GetChannelId();
  m_dmx_active->Weight(SUBSCRIPTION_WEIGHT_POSTTUNING);

  m_playingLiveStream = oldest->Open(chn.GetUniqueId(), SUBSCRIPTION_WEIGHT_NORMAL);
  m_dmx_active = oldest;
  if (m_playingLiveStream)
    PredictiveTune(prevId, chn.GetUniqueId());

  return m_playingLiveStream;
}

DEMUX_PACKET* CTvheadend::DemuxRead()
{
  DEMUX_PACKET* pkt = nullptr;

  if (m_streamchange)
  {
    /* when switching to a previously used channel, we have to trigger a stream
     * change update through kodi. We don't queue that through the dmx packet
     * buffer, as we really want to use the currently queued packets for
     * immediate playback. */
    pkt = kodi::addon::CInstancePVRClient::AllocateDemuxPacket(0);
    pkt->iStreamId = DEMUX_SPECIALID_STREAMCHANGE;
    m_streamchange = false;
    return pkt;
  }

  for (auto* dmx : m_dmx)
  {
    if (dmx == m_dmx_active)
      pkt = dmx->Read();
    else
      dmx->Trim();
  }
  return pkt;
}

void CTvheadend::CloseLiveStream()
{
  // If predictive tuning is active, demuxers will be closed automatically once they are expired.
  if (m_dmx.size() == 1)
    m_dmx_active->Close();

  m_playingLiveStream = false;
}

void CTvheadend::DemuxFlush()
{
  m_dmx_active->Flush();
}

void CTvheadend::DemuxAbort()
{
  // If predictive tuning is active, demuxers will be closed/aborted automatically once they are expired.
  if (m_dmx.size() == 1)
    m_dmx_active->Abort();
}

bool CTvheadend::SeekTime(double time, bool backward, double& startpts)
{
  return m_dmx_active->Seek(time, backward, startpts);
}

void CTvheadend::SetSpeed(int speed)
{
  m_dmx_active->Speed(speed);
}

void CTvheadend::FillBuffer(bool mode)
{
  m_dmx_active->FillBuffer(mode);
}

PVR_ERROR CTvheadend::GetStreamProperties(std::vector<kodi::addon::PVRStreamProperties>& streams)
{
  return m_dmx_active->CurrentStreams(streams);
}

PVR_ERROR CTvheadend::GetSignalStatus(int channelUid, kodi::addon::PVRSignalStatus& sig)
{
  return m_dmx_active->CurrentSignal(sig);
}

PVR_ERROR CTvheadend::GetDescrambleInfo(int channelUid, kodi::addon::PVRDescrambleInfo& info)
{
  return m_dmx_active->CurrentDescrambleInfo(info);
}

bool CTvheadend::IsTimeshifting()
{
  return m_dmx_active->IsTimeShifting();
}

PVR_ERROR CTvheadend::GetStreamTimes(kodi::addon::PVRStreamTimes& times)
{
  if (m_playingLiveStream)
  {
    return m_dmx_active->GetStreamTimes(times);
  }

  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (m_playingRecording)
  {
    if (m_playingRecording->GetState() == PVR_TIMER_STATE_RECORDING)
    {
      if (m_playingRecording->GetFilesStart() > 0)
      {
        times.SetPTSEnd((std::time(nullptr) - m_playingRecording->GetFilesStart()) * STREAM_TIME_BASE);
      }
      else
      {
        // Older tvh versions do not expose real recording start/stop time.
        // Remark: Following calculation does not always work. Returned end time might be to large, as the
        // recording might actually have started later than scheduled start time (server came up too late etc).
        times.SetPTSEnd((m_playingRecording->GetStartExtra() * 60 + std::time(nullptr) -
                         m_playingRecording->GetStart()) *
                        STREAM_TIME_BASE);
      }
    }
    else
    {
      if (m_playingRecording->GetFilesStart() > 0 && m_playingRecording->GetFilesStop() > 0)
      {
        times.SetPTSEnd((m_playingRecording->GetFilesStop() - m_playingRecording->GetFilesStart()) *
                        STREAM_TIME_BASE);
      }
      else
      {
        // Older tvh versions do not expose real recording start/stop time.
        // Remark: Kodi is handling finished recording's times very well on its own - in difference to
        // in-progress recording's times. Returning not implemented will make Kodi handle the stream times.
        return PVR_ERROR_NOT_IMPLEMENTED;
      }
    }
    return PVR_ERROR_NO_ERROR;
  }
  return PVR_ERROR_INVALID_PARAMETERS;
}
