/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "RecordingInputStream.h"

#include "HTSPConnection.h"
#include "HTSPDemuxer.h"
#include "InstanceSettings.h"

#include <algorithm>
#include <chrono>

using namespace tvheadend;

RecordingInputStream::RecordingInputStream(const kodi::addon::IInstanceInfo& instance,
                                           std::shared_ptr<RecordingContext> context)
  : CInstanceInputStream(instance),
    m_context(std::move(context)),
    m_connection(new HTSPConnection(m_context->settings, *this)),
    m_demuxer(new HTSPDemuxer(m_context->settings, *this, *m_connection, true))
{
}

RecordingInputStream::~RecordingInputStream()
{
  Close();
}

void RecordingInputStream::GetCapabilities(kodi::addon::InputstreamCapabilities& capabilities)
{
  capabilities.SetMask(INPUTSTREAM_SUPPORTS_IDEMUX | INPUTSTREAM_SUPPORTS_SEEK |
                       INPUTSTREAM_SUPPORTS_PAUSE | INPUTSTREAM_SUPPORTS_ITIME);
}

bool RecordingInputStream::Open(const kodi::addon::InputstreamProperty& properties)
{
  const auto values = properties.GetProperties();
  const auto recording = values.find("pvr.hts.recording");
  const auto profile = values.find("pvr.hts.profile");
  if (recording == values.end() || profile == values.end())
    return false;
  uint32_t id = 0;
  const auto& text = recording->second;
  if (!ParseRecordingId(text, id))
    return false;
  {
    std::lock_guard<std::mutex> lock(m_context->mutex);
    if (!m_context->alive)
      return false;
  }
  m_connection->Start();
  m_demuxer->SetStreamingProfile(profile->second);
  if (!m_demuxer->OpenRecording(id))
  {
    Close();
    return false;
  }
  m_open = true;
  std::unique_lock<std::mutex> lock(m_readyMutex);
  const bool ready = m_readyCond.wait_for(
      lock, std::chrono::milliseconds(m_context->settings->GetResponseTimeout()),
      [this]
      {
        std::vector<unsigned int> ids;
        return m_demuxer->HasEnded() || (GetStreamIds(ids) && !ids.empty());
      });
  lock.unlock();
  if (!ready || m_demuxer->HasEnded())
  {
    Close();
    return false;
  }
  return true;
}

void RecordingInputStream::Close()
{
  m_open = false;
  m_restore = false;
  m_demuxer->Close();
  m_connection->Stop();
  m_connection->StopThread();
}

bool RecordingInputStream::GetStreamIds(std::vector<unsigned int>& ids)
{
  ids.clear();
  std::vector<kodi::addon::PVRStreamProperties> streams;
  m_demuxer->CurrentStreams(streams);
  for (const auto& stream : streams)
    ids.emplace_back(stream.GetPID());
  return true;
}

bool RecordingInputStream::GetStream(int id, kodi::addon::InputstreamInfo& info)
{
  std::vector<kodi::addon::PVRStreamProperties> streams;
  m_demuxer->CurrentStreams(streams);
  const auto stream = std::find_if(streams.begin(), streams.end(),
                                   [id](const auto& value) { return value.GetPID() == id; });
  std::string codec;
  if (stream == streams.end() || !m_demuxer->GetCodecName(id, codec))
    return false;
  if (stream->GetCodecType() == PVR_CODEC_TYPE_VIDEO)
    info.SetStreamType(INPUTSTREAM_TYPE_VIDEO);
  else if (stream->GetCodecType() == PVR_CODEC_TYPE_AUDIO)
    info.SetStreamType(INPUTSTREAM_TYPE_AUDIO);
  else if (stream->GetCodecType() == PVR_CODEC_TYPE_SUBTITLE)
    info.SetStreamType(INPUTSTREAM_TYPE_SUBTITLE);
  else
    return false;
  info.SetCodecName(codec);
  info.SetExtraData(m_demuxer->GetCodecData(id));
  info.SetPhysicalIndex(id);
  info.SetLanguage(stream->GetLanguage());
  info.SetFpsScale(stream->GetFPSScale());
  info.SetFpsRate(stream->GetFPSRate());
  info.SetWidth(stream->GetWidth());
  info.SetHeight(stream->GetHeight());
  info.SetAspect(stream->GetAspect());
  info.SetChannels(stream->GetChannels());
  info.SetSampleRate(stream->GetSampleRate());
  info.SetBitRate(stream->GetBitRate());
  info.SetBitsPerSample(stream->GetBitsPerSample());
  return true;
}

bool RecordingInputStream::OpenStream(int id)
{
  kodi::addon::InputstreamInfo info;
  if (!GetStream(id, info))
    return false;
  EnableStream(id, true);
  return true;
}

void RecordingInputStream::EnableStream(int id, bool enable)
{
  std::lock_guard<std::mutex> lock(m_streamMutex);
  if (enable)
    m_disabledStreams.erase(id);
  else
    m_disabledStreams.insert(id);
}

DEMUX_PACKET* RecordingInputStream::DemuxRead()
{
  for (;;)
  {
    DEMUX_PACKET* packet = m_demuxer->Read();
    if (!packet)
      return nullptr;
    std::lock_guard<std::mutex> lock(m_streamMutex);
    if (!m_disabledStreams.count(packet->iStreamId))
      return packet;
    FreeDemuxPacket(packet);
  }
}

bool RecordingInputStream::DemuxSeekTime(double time, bool backwards, double& startpts)
{
  return m_demuxer->Seek(time, backwards, startpts);
}

void RecordingInputStream::DemuxSetSpeed(int speed)
{
  m_demuxer->Speed(speed);
}

void RecordingInputStream::DemuxAbort()
{
  m_demuxer->Abort();
}

void RecordingInputStream::DemuxFlush()
{
  m_demuxer->Flush();
}

bool RecordingInputStream::GetTimes(kodi::addon::InputstreamTimes& times)
{
  kodi::addon::PVRStreamTimes value;
  m_demuxer->GetStreamTimes(value);
  times.SetStartTime(value.GetStartTime());
  times.SetPtsStart(value.GetPTSStart());
  times.SetPtsBegin(value.GetPTSBegin());
  times.SetPtsEnd(value.GetPTSEnd());
  return true;
}

kodi::addon::PVRCodec RecordingInputStream::GetCodecByName(const std::string& name) const
{
  std::lock_guard<std::mutex> lock(m_context->mutex);
  return m_context->alive && m_context->codecLookup ? m_context->codecLookup(name)
                                                    : kodi::addon::PVRCodec();
}

DEMUX_PACKET* RecordingInputStream::AllocateDemuxPacket(int size)
{
  return CInstanceInputStream::AllocateDemuxPacket(size);
}

void RecordingInputStream::FreeDemuxPacket(DEMUX_PACKET* packet)
{
  CInstanceInputStream::FreeDemuxPacket(packet);
}

void RecordingInputStream::Disconnected()
{
  if (m_open && !m_demuxer->HasEnded())
  {
    m_restore = true;
    m_demuxer->SuspendRecording();
  }
  std::lock_guard<std::mutex> lock(m_readyMutex);
  m_readyCond.notify_all();
}

bool RecordingInputStream::Connected(std::unique_lock<std::recursive_mutex>& lock)
{
  if (!m_connection->HasCapability(RECORDING_STREAMING_CAPABILITY))
  {
    if (m_open)
      m_demuxer->EndRecording();
    return false;
  }
  if (m_restore.exchange(false) && !m_demuxer->RestartRecording(lock))
  {
    m_demuxer->EndRecording();
    return false;
  }
  return true;
}

bool RecordingInputStream::ProcessMessage(const std::string& method, htsmsg_t* message)
{
  uint32_t id = 0;
  if (!htsmsg_get_u32(message, "subscriptionId", &id) && id == m_demuxer->GetSubscriptionId())
  {
    m_demuxer->ProcessMessage(method, message);
    if (method == "subscriptionStart" || method == "subscriptionStop" ||
        method == "subscriptionSkip")
    {
      std::lock_guard<std::mutex> lock(m_readyMutex);
      m_readyCond.notify_all();
    }
  }
  return true;
}

void RecordingInputStream::ConnectionStateChange(const std::string& connection,
                                                 PVR_CONNECTION_STATE state,
                                                 const std::string& message)
{
}
