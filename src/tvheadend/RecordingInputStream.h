/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */
#pragma once

#include "IHTSPConnectionListener.h"
#include "IHTSPDemuxPacketHandler.h"
#include "RecordingContext.h"

#include "kodi/addon-instance/Inputstream.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <set>

namespace tvheadend
{
class HTSPConnection;
class HTSPDemuxer;

class ATTR_DLL_LOCAL RecordingInputStream : public kodi::addon::CInstanceInputStream,
                                            public IHTSPConnectionListener,
                                            public IHTSPDemuxPacketHandler
{
public:
  RecordingInputStream(const kodi::addon::IInstanceInfo& instance,
                       std::shared_ptr<RecordingContext> context);
  ~RecordingInputStream() override;

  void GetCapabilities(kodi::addon::InputstreamCapabilities& capabilities) override;
  bool Open(const kodi::addon::InputstreamProperty& properties) override;
  void Close() override;
  bool IsRealTimeStream() override { return false; }
  bool GetStreamIds(std::vector<unsigned int>& ids) override;
  bool GetStream(int id, kodi::addon::InputstreamInfo& info) override;
  bool OpenStream(int id) override;
  void EnableStream(int id, bool enable) override;
  DEMUX_PACKET* DemuxRead() override;
  bool DemuxSeekTime(double time, bool backwards, double& startpts) override;
  void DemuxSetSpeed(int speed) override;
  void DemuxAbort() override;
  void DemuxFlush() override;
  bool GetTimes(kodi::addon::InputstreamTimes& times) override;

  kodi::addon::PVRCodec GetCodecByName(const std::string& name) const override;
  DEMUX_PACKET* AllocateDemuxPacket(int size) override;
  void FreeDemuxPacket(DEMUX_PACKET* packet) override;

  void Disconnected() override;
  bool Connected(std::unique_lock<std::recursive_mutex>& lock) override;
  bool ProcessMessage(const std::string& method, htsmsg_t* message) override;
  void ConnectionStateChange(const std::string& connection,
                             PVR_CONNECTION_STATE state,
                             const std::string& message) override;

private:
  std::shared_ptr<RecordingContext> m_context;
  std::unique_ptr<HTSPConnection> m_connection;
  std::unique_ptr<HTSPDemuxer> m_demuxer;
  std::atomic<bool> m_open = false;
  std::atomic<bool> m_restore = false;
  std::mutex m_streamMutex;
  std::set<int> m_disabledStreams;
  std::mutex m_readyMutex;
  std::condition_variable m_readyCond;
};
} // namespace tvheadend
