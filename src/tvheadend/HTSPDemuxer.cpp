/*
 *  Copyright (C) 2017-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2014 Adam Sutton
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "HTSPDemuxer.h"

#include "HTSPConnection.h"
#include "InstanceSettings.h"
#include "utilities/Logger.h"
#include "utilities/RDSExtractor.h"

#include "kodi/addon-instance/PVR.h"

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <ctime>

#define TVH_TO_DVD_TIME(x) (static_cast<double>(x) * STREAM_TIME_BASE / 1000000.0f)

#define INVALID_SEEKTIME (-1)
#define SPEED_NORMAL (1000) // x1 playback speed

// Not all streams reported to Kodi are directly from tvh, some are created by pvr.hts.
// We need a unique stream index for every stream - tvh-supplied and pvr.hts-created.
// Easiest way is to add a fixed offset to all stream indexes delivered by tvh and to
// use numbers less than TVH_STREAM_INDEX_OFFSET for streams created by pvr.hts.
static const int TVH_STREAM_INDEX_OFFSET = 1000;

using namespace tvheadend;
using namespace tvheadend::utilities;

HTSPDemuxer::HTSPDemuxer(const std::shared_ptr<InstanceSettings>& settings,
                         IHTSPDemuxPacketHandler& demuxPktHdl,
                         HTSPConnection& conn)
  : m_settings(settings),
    m_conn(conn),
    m_pktBuffer(static_cast<size_t>(-1)),
    m_seektime(nullptr),
    m_subscription(conn),
    m_lastUse(0),
    m_lastPkt(0),
    m_startTime(0),
    m_rdsIdx(0),
    m_demuxPktHdl(demuxPktHdl)
{
}

HTSPDemuxer::~HTSPDemuxer()
{
}

void HTSPDemuxer::RebuildState()
{
  /* Re-subscribe */
  if (m_subscription.IsActive())
  {
    Logger::Log(LogLevel::LEVEL_DEBUG, "demux re-starting stream");

    std::unique_lock<std::recursive_mutex> lock(m_conn.Mutex());
    m_subscription.SendSubscribe(lock, 0, 0, true);
    m_subscription.SendSpeed(lock, 0, true);

    ResetStatus(false);
  }
}

/* **************************************************************************
 * Demuxer API
 * *************************************************************************/

void HTSPDemuxer::Close0(std::unique_lock<std::recursive_mutex>& lock)
{
  /* Send unsubscribe */
  if (m_subscription.IsActive())
    m_subscription.SendUnsubscribe(lock);

  /* Clear */
  Flush();
  Abort0();
}

void HTSPDemuxer::Abort0()
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  m_streams.clear();
  m_streamStat.clear();
  m_rdsIdx = 0;
  m_rdsExtractor.reset();
  m_seektime = nullptr;
}


bool HTSPDemuxer::Open(uint32_t channelId, enum eSubscriptionWeight weight)
{
  std::unique_lock<std::recursive_mutex> lock(m_conn.Mutex());
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux open");

  /* Close current stream */
  Close0(lock);

  /* Open new subscription */
  time_t lastUse = m_lastUse.load();
  time_t lastPkt = m_lastPkt.load();
  m_lastUse.store(std::time(nullptr));
  m_lastPkt = 0;
  m_subscription.SendSubscribe(lock, channelId, weight);

  /* Reset status */
  ResetStatus();

  /* Send unsubscribe if subscribing failed */
  if (!m_subscription.IsActive())
  {
    m_subscription.SendUnsubscribe(lock);
    m_lastUse.store(lastUse);
    m_lastPkt.store(lastPkt);
    return false;
  }

  return true;
}

void HTSPDemuxer::Close()
{
  std::unique_lock<std::recursive_mutex> lock(m_conn.Mutex());
  Close0(lock);
  ResetStatus();
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux close");
}

DEMUX_PACKET* HTSPDemuxer::Read()
{
  m_lastUse.store(std::time(nullptr));

  DEMUX_PACKET* pkt = nullptr;
  if (m_pktBuffer.Pop(pkt, 100))
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "demux read idx :%d pts %lf len %lld", pkt->iStreamId,
                pkt->pts, static_cast<long long>(pkt->iSize));
    m_lastPkt.store(m_lastUse.load());
    return pkt;
  }
  return m_demuxPktHdl.AllocateDemuxPacket(0);
}

void HTSPDemuxer::Flush()
{
  Logger::Log(LogLevel::LEVEL_TRACE, "demux flush");

  DEMUX_PACKET* pkt = nullptr;
  while (m_pktBuffer.Pop(pkt))
    m_demuxPktHdl.FreeDemuxPacket(pkt);
}

void HTSPDemuxer::Trim()
{
  Logger::Log(LogLevel::LEVEL_TRACE, "demux trim");

  /* reduce used buffer space to what is needed for DVDPlayer to resume
   * playback without buffering. This depends on the bitrate, so we don't set
   * this too small. */
  DEMUX_PACKET* pkt = nullptr;
  while (m_pktBuffer.Size() > 512 && m_pktBuffer.Pop(pkt))
    m_demuxPktHdl.FreeDemuxPacket(pkt);
}

void HTSPDemuxer::Abort()
{
  Logger::Log(LogLevel::LEVEL_TRACE, "demux abort");

  std::lock_guard<std::recursive_mutex> lock(m_conn.Mutex());
  Abort0();
  ResetStatus();
}

namespace tvheadend
{

class SubscriptionSeekTime
{
public:
  SubscriptionSeekTime() = default;

  ~SubscriptionSeekTime()
  {
    Set(INVALID_SEEKTIME); // ensure signal is sent
  }

  int64_t Get(std::unique_lock<std::recursive_mutex>& lock, uint32_t timeout)
  {
    m_cond.wait_for(lock, std::chrono::milliseconds(timeout), [this] { return m_flag == true; });
    m_flag = false;
    return m_seektime;
  }

  void Set(int64_t seektime)
  {
    m_seektime = seektime;
    m_flag = true;
    m_cond.notify_all();
  }

private:
  std::condition_variable_any m_cond;
  bool m_flag = false;
  int64_t m_seektime = INVALID_SEEKTIME;
};

} // namespace tvheadend

bool HTSPDemuxer::Seek(double time, bool, double& startpts)
{
  std::unique_lock<std::recursive_mutex> lock(m_conn.Mutex());

  if (!m_subscription.IsActive())
    return false;

  SubscriptionSeekTime seekTime;
  m_seektime = &seekTime;

  if (!m_subscription.SendSeek(lock, time))
    return false;

  /* Wait for time */
  int64_t seekedTo = (*m_seektime).Get(lock, m_settings->GetResponseTimeout());
  m_seektime = nullptr;

  if (seekedTo == INVALID_SEEKTIME)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "demux seek: invalid seek time (%lf)", time);
    Flush(); /* try to resync */
    return false;
  }

  /* Store */
  startpts = TVH_TO_DVD_TIME(seekedTo);
  Logger::Log(LogLevel::LEVEL_TRACE, "demux seek startpts = %lf", startpts);

  return true;
}

void HTSPDemuxer::Speed(int speed)
{
  std::unique_lock<std::recursive_mutex> lock(m_conn.Mutex());

  if (!m_subscription.IsActive())
    return;

  if (speed != 0)
  {
    speed = SPEED_NORMAL;
    m_lastPkt = 0;
  }

  if ((speed != m_requestedSpeed || speed == 0) && m_actualSpeed == m_subscription.GetSpeed())
  {
    m_subscription.SendSpeed(lock, speed);
  }

  m_requestedSpeed = speed;
}

void HTSPDemuxer::FillBuffer(bool mode)
{
  std::unique_lock<std::recursive_mutex> lock(m_conn.Mutex());

  if (!m_subscription.IsActive())
    return;

  int speed = (!mode || IsRealTimeStream()) ? SPEED_NORMAL : 4 * SPEED_NORMAL;

  if (speed != m_requestedSpeed && m_actualSpeed == m_subscription.GetSpeed())
  {
    m_subscription.SendSpeed(lock, speed);
  }

  m_requestedSpeed = speed;
}

void HTSPDemuxer::Weight(enum eSubscriptionWeight weight)
{
  std::unique_lock<std::recursive_mutex> lock(m_conn.Mutex());

  if (!m_subscription.IsActive() || m_subscription.GetWeight() == static_cast<uint32_t>(weight))
    return;

  m_subscription.SendWeight(lock, static_cast<uint32_t>(weight));
}

PVR_ERROR HTSPDemuxer::CurrentStreams(std::vector<kodi::addon::PVRStreamProperties>& streams)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  streams = m_streams;

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR HTSPDemuxer::CurrentSignal(kodi::addon::PVRSignalStatus& sig)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  sig.SetAdapterName(m_sourceInfo.si_adapter);
  sig.SetServiceName(m_sourceInfo.si_service);
  sig.SetProviderName(m_sourceInfo.si_provider);
  sig.SetMuxName(m_sourceInfo.si_mux);

  sig.SetAdapterStatus(m_signalInfo.fe_status);
  sig.SetSNR(m_signalInfo.fe_snr);
  sig.SetSignal(m_signalInfo.fe_signal);
  sig.SetBER(m_signalInfo.fe_ber);
  sig.SetUNC(m_signalInfo.fe_unc);

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR HTSPDemuxer::CurrentDescrambleInfo(kodi::addon::PVRDescrambleInfo& info)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  info.SetPID(m_descrambleInfo.GetPid());
  info.SetCAID(m_descrambleInfo.GetCaid());
  info.SetProviderID(m_descrambleInfo.GetProvid());
  info.SetECMTime(m_descrambleInfo.GetEcmTime());
  info.SetHops(m_descrambleInfo.GetHops());

  info.SetCardSystem(m_descrambleInfo.GetCardSystem());
  info.SetReader(m_descrambleInfo.GetReader());
  info.SetFrom(m_descrambleInfo.GetFrom());
  info.SetProtocol(m_descrambleInfo.GetProtocol());

  return PVR_ERROR_NO_ERROR;
}

bool HTSPDemuxer::IsTimeShifting() const
{
  if (!m_subscription.IsActive())
    return false;

  if (m_subscription.GetSpeed() != SPEED_NORMAL)
    return true;

  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  if (m_timeshiftStatus.shift != 0)
    return true;

  return false;
}

bool HTSPDemuxer::IsRealTimeStream() const
{
  if (!m_subscription.IsActive())
    return false;

  /* Handle as real time when reading close to the EOF (10 secs) */
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  return (m_timeshiftStatus.shift < 10000000);
}

PVR_ERROR HTSPDemuxer::GetStreamTimes(kodi::addon::PVRStreamTimes& times) const
{

  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  times.SetStartTime(m_startTime);
  times.SetPTSStart(0);
  times.SetPTSBegin(TVH_TO_DVD_TIME(m_timeshiftStatus.start));
  times.SetPTSEnd(TVH_TO_DVD_TIME(m_timeshiftStatus.end));

  return PVR_ERROR_NO_ERROR;
}

uint32_t HTSPDemuxer::GetSubscriptionId() const
{
  return m_subscription.GetId();
}

uint32_t HTSPDemuxer::GetChannelId() const
{
  if (m_subscription.IsActive())
    return m_subscription.GetChannelId();

  return 0;
}

time_t HTSPDemuxer::GetLastUse() const
{
  if (m_subscription.IsActive())
    return m_lastUse.load();

  return 0;
}

bool HTSPDemuxer::IsPaused() const
{
  if (m_subscription.IsActive())
    return m_subscription.GetSpeed() == 0;

  return false;
}

void HTSPDemuxer::SetStreamingProfile(const std::string& profile)
{
  m_subscription.SetProfile(profile);
}

void HTSPDemuxer::ResetStatus(bool resetSubscriptionData /* = true */)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  m_signalInfo.Clear();
  m_descrambleInfo.Clear();
  m_timeshiftStatus.Clear();

  if (resetSubscriptionData)
  {
    m_sourceInfo.Clear(); // only send once with subscriptionStart response
    m_startTime = 0;
  }
}

/* **************************************************************************
 * Parse incoming data
 * *************************************************************************/

bool HTSPDemuxer::ProcessMessage(const std::string& method, htsmsg_t* m)
{
  /* Subscription messages */
  if (method == "muxpkt")
    ParseMuxPacket(m);
  else if (method == "subscriptionStatus")
    m_subscription.ParseSubscriptionStatus(m);
  else if (method == "queueStatus")
    ParseQueueStatus(m);
  else if (method == "signalStatus")
    ParseSignalStatus(m);
  else if (method == "timeshiftStatus")
    ParseTimeshiftStatus(m);
  else if (method == "descrambleInfo")
    ParseDescrambleInfo(m);
  else if (method == "subscriptionStart")
    ParseSubscriptionStart(m);
  else if (method == "subscriptionStop")
    ParseSubscriptionStop(m);
  else if (method == "subscriptionSkip")
    ParseSubscriptionSkip(m);
  else if (method == "subscriptionSpeed")
    ParseSubscriptionSpeed(m);
  else if (method == "subscriptionGrace")
    ParseSubscriptionGrace(m);
  else
    Logger::Log(LogLevel::LEVEL_DEBUG, "demux unhandled subscription message [%s]", method.c_str());

  return true;
}

void HTSPDemuxer::ProcessRDS(uint32_t idx, const void* bin, size_t binlen)
{
  if (idx != m_rdsIdx)
    return;

  if (!m_rdsExtractor)
    return;

  const uint8_t rdslen = m_rdsExtractor->Decode(static_cast<const uint8_t*>(bin), binlen);
  if (rdslen > 0)
  {
    const uint32_t rdsIdx = idx - TVH_STREAM_INDEX_OFFSET;
    if (m_streamStat.find(rdsIdx) == m_streamStat.end())
    {
      // No RDS stream yet. Create and announce it.
      if (!AddRDSStream(idx, rdsIdx))
      {
        m_rdsExtractor->Reset();
        return;
      }

      // Update streams.
      Logger::Log(LogLevel::LEVEL_DEBUG, "demux stream change");

      DEMUX_PACKET* pktSpecial = m_demuxPktHdl.AllocateDemuxPacket(0);
      pktSpecial->iStreamId = DEMUX_SPECIALID_STREAMCHANGE;
      m_pktBuffer.Push(pktSpecial);
    }

    DEMUX_PACKET* pkt = m_demuxPktHdl.AllocateDemuxPacket(rdslen);
    if (!pkt)
    {
      m_rdsExtractor->Reset();
      return;
    }

    std::memcpy(pkt->pData, m_rdsExtractor->GetRDSData(), rdslen);
    pkt->iSize = rdslen;
    pkt->iStreamId = rdsIdx;

    m_pktBuffer.Push(pkt);
  }
  m_rdsExtractor->Reset();
}

void HTSPDemuxer::ParseMuxPacket(htsmsg_t* m)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  /* Ignore packets while switching channels */
  if (!m_subscription.IsActive())
  {
    Logger::Log(LogLevel::LEVEL_DEBUG, "Ignored mux packet due to channel switch");
    return;
  }

  /* Validate fields */
  uint32_t idx = 0;
  const void* bin = nullptr;
  size_t binlen = 0;
  if (htsmsg_get_u32(m, "stream", &idx) || htsmsg_get_bin(m, "payload", &bin, &binlen))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed muxpkt: 'stream'/'payload' missing");
    return;
  }

  idx += TVH_STREAM_INDEX_OFFSET;

  /* Drop packets for unknown streams */
  if (m_streamStat.find(idx) == m_streamStat.end())
  {
    Logger::Log(LogLevel::LEVEL_DEBUG, "Dropped packet with unknown stream index %i", idx);
    return;
  }

  /* Record */
  m_streamStat[idx]++;

  /* Allocate buffer */
  DEMUX_PACKET* pkt = m_demuxPktHdl.AllocateDemuxPacket(binlen);
  if (!pkt)
    return;

  std::memcpy(pkt->pData, bin, binlen);
  pkt->iSize = binlen;
  pkt->iStreamId = idx;

  /* Duration */
  uint32_t u32 = 0;
  if (!htsmsg_get_u32(m, "duration", &u32))
    pkt->duration = TVH_TO_DVD_TIME(u32);

  /* Timestamps */
  int64_t s64 = 0;
  if (!htsmsg_get_s64(m, "dts", &s64))
    pkt->dts = TVH_TO_DVD_TIME(s64);
  else
    pkt->dts = STREAM_NOPTS_VALUE;

  if (!htsmsg_get_s64(m, "pts", &s64))
    pkt->pts = TVH_TO_DVD_TIME(s64);
  else
    pkt->pts = STREAM_NOPTS_VALUE;

  /* Type (for debug only) */
  char type = 0;
  if (!htsmsg_get_u32(m, "frametype", &u32))
    type = static_cast<char>(u32);
  if (!type)
    type = '_';

  bool ignore = m_seektime != nullptr;

  Logger::Log(LogLevel::LEVEL_TRACE, "demux pkt idx %d:%d type %c pts %lf len %lld%s", idx,
              pkt->iStreamId, type, pkt->pts, static_cast<long long>(binlen),
              ignore ? " IGNORE" : "");

  /* Store */
  if (!ignore)
  {
    if (m_startTime == 0)
    {
      // first paket for this subscription
      m_startTime = std::time(nullptr);
    }
    m_pktBuffer.Push(pkt);

    // Process RDS data, if present.
    ProcessRDS(idx, bin, binlen);
  }
  else
    m_demuxPktHdl.FreeDemuxPacket(pkt);
}

bool HTSPDemuxer::AddRDSStream(uint32_t audioIdx, uint32_t rdsIdx)
{
  for (const auto& stream : m_streams)
  {
    if (stream.GetPID() != audioIdx)
      continue;

    // Found the stream with the embedded RDS data. Create corresponding RDS stream.
    kodi::addon::PVRCodec codec = m_demuxPktHdl.GetCodecByName("rds");
    if (codec.GetCodecType() == PVR_CODEC_TYPE_UNKNOWN)
      return false;

    m_streamStat[rdsIdx] = 0;

    kodi::addon::PVRStreamProperties rdsStream;
    rdsStream.SetCodecType(codec.GetCodecType());
    rdsStream.SetCodecId(codec.GetCodecId());
    rdsStream.SetPID(rdsIdx);
    rdsStream.SetLanguage(stream.GetLanguage());

    // We can only use PVR_STREAM_MAX_STREAMS streams
    if (m_streams.size() < PVR_STREAM_MAX_STREAMS)
    {
      Logger::Log(LogLevel::LEVEL_DEBUG, "Adding rds stream. id: %d", rdsIdx);
      m_streams.emplace_back(rdsStream);
      return true;
    }
    else
    {
      Logger::Log(LogLevel::LEVEL_INFO,
                  "Maximum stream limit reached ignoring id: %d, type rds, codec: %u", rdsIdx,
                  rdsStream.GetCodecId());
      return false;
    }
  }
  // stream with embedded RDS data not found
  return false;
}

bool HTSPDemuxer::AddTVHStream(uint32_t idx, const char* type, htsmsg_field_t* f)
{
  std::string codecName;
  if (!strcmp(type, "MPEG2AUDIO"))
    codecName = "MP2";
  else if (!strcmp(type, "MPEGTS"))
    codecName = "MPEG2VIDEO";
  else if (!strcmp(type, "TEXTSUB"))
    codecName = "TEXT";
  else
    codecName = type;

  kodi::addon::PVRCodec codec = m_demuxPktHdl.GetCodecByName(codecName);
  if (codec.GetCodecType() == PVR_CODEC_TYPE_UNKNOWN)
    return false;

  m_streamStat[idx] = 0;

  kodi::addon::PVRStreamProperties stream;
  stream.SetCodecType(codec.GetCodecType());
  stream.SetCodecId(codec.GetCodecId());
  stream.SetPID(idx);

  /* Subtitle ID */
  if ((stream.GetCodecType() == PVR_CODEC_TYPE_SUBTITLE) && !std::strcmp("DVBSUB", type))
  {
    uint32_t composition_id = 0, ancillary_id = 0;
    htsmsg_get_u32(&f->hmf_msg, "composition_id", &composition_id);
    htsmsg_get_u32(&f->hmf_msg, "ancillary_id", &ancillary_id);
    stream.SetSubtitleInfo((composition_id & 0xffff) | ((ancillary_id & 0xffff) << 16));
  }

  /* Language */
  if (stream.GetCodecType() == PVR_CODEC_TYPE_SUBTITLE ||
      stream.GetCodecType() == PVR_CODEC_TYPE_AUDIO || stream.GetCodecType() == PVR_CODEC_TYPE_RDS)
  {
    const char* language = htsmsg_get_str(&f->hmf_msg, "language");
    if (language)
      stream.SetLanguage(language);
  }

  /* RDS */
  if (stream.GetCodecType() == PVR_CODEC_TYPE_RDS)
  {
    // Extra RDS streams have higher priority compared to RDS data enclosed in audio streams.
    m_rdsIdx = idx;
    m_rdsExtractor.reset();
  }

  /* Audio data */
  if (stream.GetCodecType() == PVR_CODEC_TYPE_AUDIO)
  {
    stream.SetChannels(htsmsg_get_u32_or_default(&f->hmf_msg, "channels", 2));
    stream.SetSampleRate(htsmsg_get_u32_or_default(&f->hmf_msg, "rate", 48000));

    if (m_rdsIdx == 0)
    {
      // MPEG2 and AAC audio streams may contain embedded RDS data.
      // For older tvhs, which do not send the rds flag via HTSP, default
      // to 1 and try to find RDS data in the submitted mux pakets later.
      const uint32_t rds = htsmsg_get_u32_or_default(&f->hmf_msg, "rds_uecp", 1);
      if (rds != 0)
      {
        if (std::strcmp("MPEG2AUDIO", type) == 0)
        {
          m_rdsIdx = idx;
          m_rdsExtractor.reset(new utilities::RDSExtractorMP2());
        }
        else if (std::strcmp("AAC", type) == 0)
        {
          m_rdsIdx = idx;
          m_rdsExtractor.reset(new utilities::RDSExtractorAAC());
        }
      }
    }
  }

  /* Video */
  if (stream.GetCodecType() == PVR_CODEC_TYPE_VIDEO)
  {
    stream.SetWidth(htsmsg_get_u32_or_default(&f->hmf_msg, "width", 0));
    stream.SetHeight(htsmsg_get_u32_or_default(&f->hmf_msg, "height", 0));

    /* Ignore this message if the stream details haven't been determined
       yet, a new message will be sent once they have. This is fixed in
       some versions of tvheadend and is here for backward compatibility. */
    if (stream.GetWidth() == 0 || stream.GetHeight() == 0)
    {
      Logger::Log(LogLevel::LEVEL_DEBUG, "Ignoring subscriptionStart, stream details missing");
      return false;
    }

    /* Setting aspect ratio to zero will cause XBMC to handle changes in it */
    stream.SetAspect(0.0f);

    uint32_t duration = htsmsg_get_u32_or_default(&f->hmf_msg, "duration", 0);
    if (duration > 0)
    {
      stream.SetFPSScale(duration);
      stream.SetFPSRate(STREAM_TIME_BASE);
    }
  }

  /* We can only use PVR_STREAM_MAX_STREAMS streams */
  if (m_streams.size() < PVR_STREAM_MAX_STREAMS)
  {
    Logger::Log(LogLevel::LEVEL_DEBUG, "  id: %d, type %s, codec: %u", idx, type,
                stream.GetCodecId());
    m_streams.emplace_back(stream);
    return true;
  }
  else
  {
    Logger::Log(LogLevel::LEVEL_INFO,
                "Maximum stream limit reached ignoring id: %d, type %s, codec: %u", idx, type,
                stream.GetCodecId());
    return false;
  }
}

void HTSPDemuxer::ParseSubscriptionStart(htsmsg_t* m)
{
  /* Validate */
  htsmsg_t* l = htsmsg_get_list(m, "streams");
  if (!l)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed subscriptionStart: 'streams' missing");
    return;
  }

  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  m_streamStat.clear();
  m_streams.clear();
  m_rdsIdx = 0;
  m_rdsExtractor.reset();

  Logger::Log(LogLevel::LEVEL_DEBUG, "demux subscription start");

  /* Process each */
  htsmsg_field_t* f = nullptr;
  HTSMSG_FOREACH(f, l)
  {
    if (f->hmf_type != HMF_MAP)
      continue;

    const char* type = htsmsg_get_str(&f->hmf_msg, "type");
    if (!type)
      continue;

    uint32_t idx = 0;
    if (htsmsg_get_u32(&f->hmf_msg, "index", &idx))
      continue;

    idx += TVH_STREAM_INDEX_OFFSET;
    AddTVHStream(idx, type, f);
  }

  /* Update streams */
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux stream change");

  DEMUX_PACKET* pkt = m_demuxPktHdl.AllocateDemuxPacket(0);
  pkt->iStreamId = DEMUX_SPECIALID_STREAMCHANGE;
  m_pktBuffer.Push(pkt);

  /* Source data */
  ParseSourceInfo(htsmsg_get_map(m, "sourceinfo"));
}

void HTSPDemuxer::ParseSourceInfo(htsmsg_t* m)
{
  /* Ignore */
  if (!m)
    return;

  Logger::Log(LogLevel::LEVEL_TRACE, "demux sourceInfo:");

  /* include position in mux name
   * as users might receive multiple satellite positions */
  m_sourceInfo.si_mux.clear();

  const char* str = htsmsg_get_str(m, "satpos");
  if (str)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  satpos : %s", str);
    m_sourceInfo.si_mux.append(str);
    m_sourceInfo.si_mux.append(": ");
  }

  str = htsmsg_get_str(m, "mux");
  if (str)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  mux     : %s", str);
    m_sourceInfo.si_mux.append(str);
  }

  str = htsmsg_get_str(m, "adapter");
  if (str)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  adapter : %s", str);
    m_sourceInfo.si_adapter = str;
  }

  str = htsmsg_get_str(m, "network");
  if (str)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  network : %s", str);
    m_sourceInfo.si_network = str;
  }

  str = htsmsg_get_str(m, "provider");
  if (str)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  provider : %s", str);
    m_sourceInfo.si_provider = str;
  }

  str = htsmsg_get_str(m, "service");
  if (str)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  service : %s", str);
    m_sourceInfo.si_service = str;
  }
}

void HTSPDemuxer::ParseSubscriptionStop(htsmsg_t*)
{
}

void HTSPDemuxer::ParseSubscriptionSkip(htsmsg_t* m)
{
  std::lock_guard<std::recursive_mutex> lock(m_conn.Mutex());

  if (m_seektime == nullptr)
    return;

  int64_t s64 = 0;
  if (htsmsg_get_s64(m, "time", &s64))
  {
    (*m_seektime).Set(INVALID_SEEKTIME);
  }
  else
  {
    (*m_seektime).Set(s64 < 0 ? 0 : s64);
    Flush(); /* flush old packets (with wrong pts) */
  }
}

void HTSPDemuxer::ParseSubscriptionSpeed(htsmsg_t* m)
{
  int32_t s32 = 0;
  if (!htsmsg_get_s32(m, "speed", &s32))
    Logger::Log(LogLevel::LEVEL_TRACE, "recv speed %d", s32);

  std::lock_guard<std::recursive_mutex> lock(m_conn.Mutex());
  m_actualSpeed = s32 * 10;
}

void HTSPDemuxer::ParseSubscriptionGrace(htsmsg_t* m)
{
}

void HTSPDemuxer::ParseQueueStatus(htsmsg_t* m)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  Logger::Log(LogLevel::LEVEL_TRACE, "stream stats:");
  for (const auto& stat : m_streamStat)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  idx:%d num:%d", stat.first, stat.second);
  }

  Logger::Log(LogLevel::LEVEL_TRACE, "queue stats:");

  uint32_t u32 = 0;
  if (!htsmsg_get_u32(m, "packets", &u32))
    Logger::Log(LogLevel::LEVEL_TRACE, "  pkts  %d", u32);
  if (!htsmsg_get_u32(m, "bytes", &u32))
    Logger::Log(LogLevel::LEVEL_TRACE, "  bytes %d", u32);
  if (!htsmsg_get_u32(m, "delay", &u32))
    Logger::Log(LogLevel::LEVEL_TRACE, "  delay %d", u32);
  if (!htsmsg_get_u32(m, "Idrops", &u32))
    Logger::Log(LogLevel::LEVEL_TRACE, "  Idrop %d", u32);
  if (!htsmsg_get_u32(m, "Pdrops", &u32))
    Logger::Log(LogLevel::LEVEL_TRACE, "  Pdrop %d", u32);
  if (!htsmsg_get_u32(m, "Bdrops", &u32))
    Logger::Log(LogLevel::LEVEL_TRACE, "  Bdrop %d", u32);
}

void HTSPDemuxer::ParseSignalStatus(htsmsg_t* m)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  /* Reset */
  m_signalInfo.Clear();

  /* Parse */
  Logger::Log(LogLevel::LEVEL_TRACE, "signalStatus:");

  const char* str = htsmsg_get_str(m, "feStatus");
  if (str)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  status : %s", str);
    m_signalInfo.fe_status = str;
  }
  else
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed signalStatus: 'feStatus' missing, ignoring");
  }

  uint32_t u32 = 0;
  if (!htsmsg_get_u32(m, "feSNR", &u32))
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  snr    : %d", u32);
    m_signalInfo.fe_snr = u32;
  }
  if (!htsmsg_get_u32(m, "feBER", &u32))
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  ber    : %d", u32);
    m_signalInfo.fe_ber = u32;
  }
  if (!htsmsg_get_u32(m, "feUNC", &u32))
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  unc    : %d", u32);
    m_signalInfo.fe_unc = u32;
  }
  if (!htsmsg_get_u32(m, "feSignal", &u32))
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  signal    : %d", u32);
    m_signalInfo.fe_signal = u32;
  }
}

void HTSPDemuxer::ParseTimeshiftStatus(htsmsg_t* m)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  /* Parse */
  Logger::Log(LogLevel::LEVEL_TRACE, "timeshiftStatus:");

  uint32_t u32 = 0;
  if (!htsmsg_get_u32(m, "full", &u32))
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  full  : %d", u32);
    m_timeshiftStatus.full = u32 == 0 ? false : true;
  }
  else
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed timeshiftStatus: 'full' missing, ignoring");
  }

  int64_t s64 = 0;
  if (!htsmsg_get_s64(m, "shift", &s64))
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  shift : %lld", s64);
    m_timeshiftStatus.shift = s64;
  }
  else
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed timeshiftStatus: 'shift' missing, ignoring");
  }
  if (!htsmsg_get_s64(m, "start", &s64))
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  start : %lld", s64);
    m_timeshiftStatus.start = s64;
  }
  if (!htsmsg_get_s64(m, "end", &s64))
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  end   : %lld", s64);
    m_timeshiftStatus.end = s64;
  }
}

void HTSPDemuxer::ParseDescrambleInfo(htsmsg_t* m)
{
  /* Parse mandatory fields */
  uint32_t pid = 0;
  uint32_t caid = 0;
  uint32_t provid = 0;
  uint32_t ecmtime = 0;
  uint32_t hops = 0;
  if (htsmsg_get_u32(m, "pid", &pid) || htsmsg_get_u32(m, "caid", &caid) ||
      htsmsg_get_u32(m, "provid", &provid) || htsmsg_get_u32(m, "ecmtime", &ecmtime) ||
      htsmsg_get_u32(m, "hops", &hops))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed descrambleInfo, mandatory parameters missing");
    return;
  }

  /* Parse optional fields */
  const char* cardsystem = htsmsg_get_str(m, "cardsystem");
  const char* reader = htsmsg_get_str(m, "reader");
  const char* from = htsmsg_get_str(m, "from");
  const char* protocol = htsmsg_get_str(m, "protocol");

  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  /* Reset */
  m_descrambleInfo.Clear();

  /* Store */
  m_descrambleInfo.SetPid(pid);
  m_descrambleInfo.SetCaid(caid);
  m_descrambleInfo.SetProvid(provid);
  m_descrambleInfo.SetEcmTime(ecmtime);
  m_descrambleInfo.SetHops(hops);

  if (cardsystem)
    m_descrambleInfo.SetCardSystem(cardsystem);
  if (reader)
    m_descrambleInfo.SetReader(reader);
  if (from)
    m_descrambleInfo.SetFrom(from);
  if (protocol)
    m_descrambleInfo.SetProtocol(protocol);

  /* Log */
  Logger::Log(LogLevel::LEVEL_TRACE, "descrambleInfo:");
  Logger::Log(LogLevel::LEVEL_TRACE, "  pid: %d", pid);
  Logger::Log(LogLevel::LEVEL_TRACE, "  caid: 0x%X", caid);
  Logger::Log(LogLevel::LEVEL_TRACE, "  provid: %d", provid);
  Logger::Log(LogLevel::LEVEL_TRACE, "  ecmtime: %d", ecmtime);
  Logger::Log(LogLevel::LEVEL_TRACE, "  hops: %d", hops);
  Logger::Log(LogLevel::LEVEL_TRACE, "  cardsystem: %s", cardsystem);
  Logger::Log(LogLevel::LEVEL_TRACE, "  reader: %s", reader);
  Logger::Log(LogLevel::LEVEL_TRACE, "  from: %s", from);
  Logger::Log(LogLevel::LEVEL_TRACE, "  protocol: %s", protocol);
}
