/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

extern "C"
{
#include "libhts/htsmsg.h"

#include <sys/types.h>
}

#include "tvheadend/AutoRecordings.h"
#include "tvheadend/ChannelTuningPredictor.h"
#include "tvheadend/HTSPMessage.h"
#include "tvheadend/IHTSPConnectionListener.h"
#include "tvheadend/IHTSPDemuxPacketHandler.h"
#include "tvheadend/Profile.h"
#include "tvheadend/TimeRecordings.h"
#include "tvheadend/entity/Channel.h"
#include "tvheadend/entity/Recording.h"
#include "tvheadend/entity/Schedule.h"
#include "tvheadend/entity/Tag.h"
#include "tvheadend/utilities/AsyncState.h"

#include "kodi/addon-instance/PVR.h"
#include "p8-platform/threads/threads.h"
#include "p8-platform/util/buffer.h"

#include <string>
#include <utility>
#include <vector>

#define UNNUMBERED_CHANNEL (10000)

/*
 * Forward decleration of classes
 */
namespace tvheadend
{
class HTSPConnection;
class HTSPDemuxer;
class HTSPVFS;
} // namespace tvheadend

/* Typedefs */
typedef P8PLATFORM::SyncedBuffer<tvheadend::HTSPMessage> HTSPMessageQueue;

/*
 * Root object for Tvheadend connection
 */
class ATTRIBUTE_HIDDEN CTvheadend : public kodi::addon::CInstancePVRClient,
                                    public P8PLATFORM::CThread,
                                    public tvheadend::IHTSPConnectionListener,
                                    public tvheadend::IHTSPDemuxPacketHandler
{
public:
  CTvheadend(KODI_HANDLE instance, const std::string& kodiVersion);
  ~CTvheadend() override;

  void Start();
  void Stop();

  // IHTSPConnectionListener implementation
  void Disconnected() override;
  bool Connected() override;
  bool ProcessMessage(const std::string& method, htsmsg_t* msg) override;
  void ConnectionStateChange(const std::string& connectionString,
                             PVR_CONNECTION_STATE newState,
                             const std::string& message) override;

  // IHTSPDemuxPacketHandler implementation
  kodi::addon::PVRCodec GetCodecByName(const std::string& codecName) const override;
  DemuxPacket* AllocateDemuxPacket(int iDataSize) override;
  void FreeDemuxPacket(DemuxPacket* pPacket) override;

  const tvheadend::entity::Channels& GetChannels() const { return m_channels; }

  PVR_ERROR GetCapabilities(kodi::addon::PVRCapabilities& capabilities) override;
  PVR_ERROR GetDriveSpace(uint64_t& total, uint64_t& used) override;

  PVR_ERROR GetChannelGroupsAmount(int& amount) override;
  PVR_ERROR GetChannelGroups(bool radio, kodi::addon::PVRChannelGroupsResultSet& results) override;
  PVR_ERROR GetChannelGroupMembers(const kodi::addon::PVRChannelGroup& group,
                                   kodi::addon::PVRChannelGroupMembersResultSet& results) override;

  PVR_ERROR GetChannelsAmount(int& amount) override;
  PVR_ERROR GetChannels(bool radio, kodi::addon::PVRChannelsResultSet& results) override;
  PVR_ERROR GetChannelStreamProperties(const kodi::addon::PVRChannel& channel, std::vector<kodi::addon::PVRStreamProperty>& properties) override;

  PVR_ERROR GetRecordingsAmount(bool deleted, int& amount) override;
  PVR_ERROR GetRecordings(bool deleted, kodi::addon::PVRRecordingsResultSet& results) override;
  PVR_ERROR GetRecordingEdl(const kodi::addon::PVRRecording& rec,
                            std::vector<kodi::addon::PVREDLEntry>& edl) override;
  PVR_ERROR DeleteRecording(const kodi::addon::PVRRecording& rec) override;
  PVR_ERROR RenameRecording(const kodi::addon::PVRRecording& rec) override;
  PVR_ERROR SetRecordingLifetime(const kodi::addon::PVRRecording& rec) override;
  PVR_ERROR SetRecordingPlayCount(const kodi::addon::PVRRecording& rec, int playCount) override;
  PVR_ERROR SetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& rec,
                                           int playposition) override;
  PVR_ERROR GetRecordingLastPlayedPosition(const kodi::addon::PVRRecording& rec,
                                           int& playposition) override;
  PVR_ERROR GetTimerTypes(std::vector<kodi::addon::PVRTimerType>& types) override;
  PVR_ERROR GetTimersAmount(int& amount) override;
  PVR_ERROR GetTimers(kodi::addon::PVRTimersResultSet& results) override;
  PVR_ERROR AddTimer(const kodi::addon::PVRTimer& timer) override;
  PVR_ERROR DeleteTimer(const kodi::addon::PVRTimer& timer, bool forceDelete) override;
  PVR_ERROR UpdateTimer(const kodi::addon::PVRTimer& timer) override;

  PVR_ERROR GetEPGForChannel(int channelUid,
                             time_t start,
                             time_t end,
                             kodi::addon::PVREPGTagsResultSet& results) override;
  PVR_ERROR SetEPGTimeFrame(int days) override;

  void GetLivetimeValues(std::vector<kodi::addon::PVRTypeIntValue>& lifetimeValues) const;

private:
  bool CreateTimer(const tvheadend::entity::Recording& tvhTmr, kodi::addon::PVRTimer& tmr);

  uint32_t GetNextUnnumberedChannelNumber();
  std::string GetImageURL(const char* str);

  /**
   * Queries the server for available streaming profiles and populates
   * m_profiles
   */
  void QueryAvailableProfiles();

  /**
   * @param streamingProfile the streaming profile to check for
   * @return whether the server supports the specified streaming profile
   */
  bool HasStreamingProfile(const std::string& streamingProfile) const;

  /*
   * Predictive tuning
   */
  void PredictiveTune(uint32_t fromChannelId, uint32_t toChannelId);
  void TuneOnOldest(uint32_t channelId);

  /*
   * Message processing (CThread implementation)
   */
  void* Process() override;

  /*
   * Event handling
   */
  void TriggerChannelGroupsUpdate();
  void TriggerChannelUpdate();
  void TriggerRecordingUpdate();
  void TriggerTimerUpdate();
  void PushEpgEventUpdate(const tvheadend::entity::Event& epg, EPG_EVENT_STATE state);

  /*
   * Epg Handling
   */
  void CreateEvent(const tvheadend::entity::Event& event, kodi::addon::PVREPGTag& epg);
  void TransferEvent(const tvheadend::entity::Event& event, EPG_EVENT_STATE state);
  void TransferEvent(kodi::addon::PVREPGTagsResultSet& results,
                     const tvheadend::entity::Event& event);

  /*
   * Message sending
   */
  PVR_ERROR SendDvrDelete(uint32_t id, const char* method);
  PVR_ERROR SendDvrUpdate(htsmsg_t* m);

  /*
   * Channel/Tags/Recordings/Events
   */
  void SyncInitCompleted();
  void SyncChannelsCompleted();
  void SyncDvrCompleted();
  void SyncEpgCompleted();
  void SyncCompleted();
  void ParseTagAddOrUpdate(htsmsg_t* m, bool bAdd);
  void ParseTagDelete(htsmsg_t* m);
  void ParseChannelAddOrUpdate(htsmsg_t* m, bool bAdd);
  void ParseChannelDelete(htsmsg_t* m);
  void ParseRecordingAddOrUpdate(htsmsg_t* m, bool bAdd);
  void ParseRecordingDelete(htsmsg_t* m);
  void ParseEventAddOrUpdate(htsmsg_t* m, bool bAdd);
  void ParseEventDelete(htsmsg_t* m);
  bool ParseEvent(htsmsg_t* msg, bool bAdd, tvheadend::entity::Event& evt);

  /*
   * VFS
   */
  bool VfsIsActiveRecording() const
  {
    return m_playingRecording && m_playingRecording->GetState() == PVR_TIMER_STATE_RECORDING;
  }

public:
  /*
   * Connection (pass-thru)
   */
  PVR_ERROR GetBackendName(std::string& name) override;
  PVR_ERROR GetBackendVersion(std::string& version) override;
  PVR_ERROR GetBackendHostname(std::string& hostname) override;
  PVR_ERROR GetConnectionString(std::string& connection) override;

  bool HasCapability(const std::string& capability) const;
  PVR_ERROR OnSystemSleep() override;
  PVR_ERROR OnSystemWake() override;

  /*
   * Demuxer
   */
  bool OpenLiveStream(const kodi::addon::PVRChannel& chn) override;
  void CloseLiveStream() override;
  DemuxPacket* DemuxRead() override;
  void DemuxFlush() override;
  void DemuxAbort() override;
  bool SeekTime(double time, bool backward, double& startpts) override;
  void SetSpeed(int speed) override;
  void FillBuffer(bool mode) override;
  PVR_ERROR GetStreamProperties(std::vector<kodi::addon::PVRStreamProperties>& streams) override;
  PVR_ERROR GetSignalStatus(int channelUid, kodi::addon::PVRSignalStatus& sig) override;
  PVR_ERROR GetDescrambleInfo(int channelUid, kodi::addon::PVRDescrambleInfo& info) override;
  bool CanPauseStream() override { return HasCapability("timeshift"); }
  bool CanSeekStream() override { return HasCapability("timeshift"); }
  bool IsTimeshifting();
  bool IsRealTimeStream() override;

  void CloseExpiredSubscriptions();

  /*
   * VFS (pass-thru)
   */
  bool OpenRecordedStream(const kodi::addon::PVRRecording& rec) override;
  void CloseRecordedStream() override;
  int ReadRecordedStream(unsigned char* buf, unsigned int len) override;
  int64_t SeekRecordedStream(int64_t position, int whence) override;
  int64_t LengthRecordedStream() override;
  void PauseStream(bool paused) override;
  PVR_ERROR GetStreamReadChunkSize(int& chunksize) override;

  /*
   * stream times (live streams and recordings)
   */
  PVR_ERROR GetStreamTimes(kodi::addon::PVRStreamTimes& times) override;

  /**
   * The streaming profiles available on the server
   */
  tvheadend::Profiles m_profiles;

  P8PLATFORM::CMutex m_mutex;

  tvheadend::HTSPConnection* m_conn;

  std::vector<tvheadend::HTSPDemuxer*> m_dmx;
  tvheadend::HTSPDemuxer* m_dmx_active;
  bool m_streamchange;
  tvheadend::HTSPVFS* m_vfs;

  HTSPMessageQueue m_queue;

  tvheadend::entity::Channels m_channels;
  tvheadend::entity::Tags m_tags;
  tvheadend::entity::Recordings m_recordings;
  tvheadend::entity::Schedules m_schedules;

  tvheadend::ChannelTuningPredictor m_channelTuningPredictor;

  tvheadend::SHTSPEventList m_events;

  tvheadend::utilities::AsyncState m_asyncState;

  tvheadend::TimeRecordings m_timeRecordings;
  tvheadend::AutoRecordings m_autoRecordings;

  int m_epgMaxDays;

  bool m_playingLiveStream;
  tvheadend::entity::Recording* m_playingRecording;
};
