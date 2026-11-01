/*
 *  Copyright (C) 2017-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <time.h>
#include <vector>

extern "C"
{
#include "libhts/htsmsg.h"
}

#include "IHTSPDemuxPacketHandler.h"
#include "Subscription.h"
#include "status/DescrambleInfo.h"
#include "status/Quality.h"
#include "status/SourceInfo.h"
#include "status/TimeshiftStatus.h"
#include "utilities/SyncedBuffer.h"

#include "kodi/addon-instance/pvr/Channels.h"
#include "kodi/addon-instance/pvr/Stream.h"

namespace tvheadend
{

class HTSPConnection;
class InstanceSettings;
class SubscriptionSeekTime;

namespace utilities
{
class RDSExtractor;
}

/*
 * HTSP Demuxer - live streams
 */
class HTSPDemuxer
{
public:
  HTSPDemuxer(const std::shared_ptr<InstanceSettings>& settings,
              IHTSPDemuxPacketHandler& demuxPktHdl,
              HTSPConnection& conn,
              bool recording = false);
  ~HTSPDemuxer();

  bool ProcessMessage(const std::string& method, htsmsg_t* m);
  void RebuildState();
  void EndRecording();
  void SuspendRecording();
  bool RestartRecording(std::unique_lock<std::recursive_mutex>& lock);
  bool HasEnded() const { return m_recording && (m_eof || m_failed); }
  bool GetCodecName(int streamId, std::string& name) const;
  std::vector<uint8_t> GetCodecData(int streamId) const;

  bool Open(uint32_t channelId,
            tvheadend::eSubscriptionWeight weight = tvheadend::SUBSCRIPTION_WEIGHT_NORMAL);
  bool OpenRecording(uint32_t dvrId);
  void Close();
  DEMUX_PACKET* Read();
  void Trim();
  void Flush();
  void Abort();
  bool Seek(double time, bool backwards, double& startpts);
  void Speed(int speed);
  void FillBuffer(bool mode);
  void Weight(tvheadend::eSubscriptionWeight weight);

  PVR_ERROR CurrentStreams(std::vector<kodi::addon::PVRStreamProperties>& streams);
  PVR_ERROR CurrentSignal(kodi::addon::PVRSignalStatus& sig);
  PVR_ERROR CurrentDescrambleInfo(kodi::addon::PVRDescrambleInfo& info);

  bool IsTimeShifting() const;
  bool IsRealTimeStream() const;
  PVR_ERROR GetStreamTimes(kodi::addon::PVRStreamTimes& times) const;

  uint32_t GetSubscriptionId() const;
  uint32_t GetChannelId() const;
  time_t GetLastUse() const;
  bool IsPaused() const;
  void Pause(bool paused);

  /**
   * Tells each demuxer to use the specified profile for new subscriptions
   * @param profile the profile to use
   */
  void SetStreamingProfile(const std::string& profile);

private:
  bool Open0(uint32_t sourceId, tvheadend::eSubscriptionWeight weight);
  void InterruptRecording();
  void Close0(std::unique_lock<std::recursive_mutex>& lock);
  void Abort0();
  void CancelSeek();
  bool Seek0(std::unique_lock<std::recursive_mutex>& lock,
             double time,
             double& startpts,
             bool restart);
  void WakeBuffer();
  void QueuePacket(DEMUX_PACKET* packet);
  void ReleaseBufferSpace(const DEMUX_PACKET* packet);

  /**
   * Resets the signal, quality, timeshift info and optionally the starttime
   * @param resetStartTime if true, all subscription-related data will be reset
   */
  void ResetStatus(bool resetSubscriptionData = true);

  void ParseMuxPacket(htsmsg_t* m);
  void ParseSourceInfo(htsmsg_t* m);
  void ParseSubscriptionStart(htsmsg_t* m);
  void ProcessSubscriptionStatus(htsmsg_t* m);
  void ParseSubscriptionStop(htsmsg_t* m);
  void ParseSubscriptionSkip(htsmsg_t* m);
  void ParseSubscriptionSpeed(htsmsg_t* m);
  void ParseSubscriptionGrace(htsmsg_t* m);
  void ParseQueueStatus(htsmsg_t* m);
  void ParseSignalStatus(htsmsg_t* m);
  void ParseTimeshiftStatus(htsmsg_t* m);
  void ParseDescrambleInfo(htsmsg_t* m);

  bool AddTVHStream(uint32_t idx, const char* type, htsmsg_field_t* f);
  bool AddRDSStream(uint32_t audioIdx, uint32_t rdsIdx);
  void ProcessRDS(uint32_t idx, const void* bin, size_t binlen);

  const bool m_recording;
  std::atomic<bool> m_closed = false;
  std::atomic<bool> m_eof = false;
  std::atomic<bool> m_failed = false;
  std::atomic<bool> m_reconnecting = false;
  std::atomic<int64_t> m_position = 0;
  int64_t m_duration = 0;
  std::mutex m_bufferMutex;
  std::condition_variable m_bufferCond;
  size_t m_bufferBytes = 0;
  mutable std::recursive_mutex m_mutex;
  std::shared_ptr<InstanceSettings> m_settings;
  HTSPConnection& m_conn;
  tvheadend::utilities::SyncedBuffer<DEMUX_PACKET*> m_pktBuffer;
  std::vector<kodi::addon::PVRStreamProperties> m_streams;
  std::map<int, int> m_streamStat;
  // PVRStreamProperties cannot carry decoder initialization data.
  std::map<int, std::string> m_codecNames;
  std::map<int, std::vector<uint8_t>> m_codecData;
  std::atomic<SubscriptionSeekTime*> m_seektime;
  tvheadend::status::SourceInfo m_sourceInfo;
  tvheadend::status::Quality m_signalInfo;
  tvheadend::status::TimeshiftStatus m_timeshiftStatus;
  tvheadend::status::DescrambleInfo m_descrambleInfo;
  tvheadend::Subscription m_subscription;
  std::atomic<time_t> m_lastUse;
  std::atomic<time_t> m_lastPkt;
  std::atomic<time_t> m_lastMessage{0};
  std::atomic<time_t> m_startTime;
  uint32_t m_rdsIdx;
  std::unique_ptr<utilities::RDSExtractor> m_rdsExtractor;
  int32_t m_requestedSpeed = 1000;
  int32_t m_actualSpeed = 1000;

  IHTSPDemuxPacketHandler& m_demuxPktHdl;
};

} // namespace tvheadend
