/*
 *      Copyright (C) 2014 Adam Sutton
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

#include "HTSPDemuxer.h"

#include "../client.h"
#include "HTSPConnection.h"
#include "Settings.h"
#include "utilities/Logger.h"
#include "xbmc_codec_descriptor.hpp"

#define TVH_TO_DVD_TIME(x) (static_cast<double>(x) * DVD_TIME_BASE / 1000000.0f)

#define INVALID_SEEKTIME (-1)
#define SPEED_NORMAL (1000) // x1 playback speed

// Not all streams reported to Kodi are directly from tvh, some are created by pvr.hts.
// We need a unique stream index for every stream - tvh-supplied and pvr.hts-created.
// Easiest way is to add a fixed offset to all stream indexes delivered by tvh and to
// use numbers less than TVH_STREAM_INDEX_OFFSET for streams created by pvr.hts.
static const int TVH_STREAM_INDEX_OFFSET = 1000;

using namespace ADDON;
using namespace P8PLATFORM;
using namespace tvheadend;
using namespace tvheadend::utilities;

HTSPDemuxer::HTSPDemuxer ( HTSPConnection &conn )
  : m_conn(conn), m_pktBuffer((size_t)-1),
    m_seekTime(INVALID_SEEKTIME),
    m_seeking(false),
    m_subscription(conn), m_lastUse(0),
    m_startTime(0), m_rdsIdx(0)
{
}

HTSPDemuxer::~HTSPDemuxer ()
{
}

void HTSPDemuxer::Connected ( void )
{
  /* Re-subscribe */
  if (m_subscription.IsActive())
  {
    Logger::Log(LogLevel::LEVEL_DEBUG, "demux re-starting stream");
    m_subscription.SendSubscribe(0, 0, true);
    m_subscription.SendSpeed(0, true);

    ResetStatus(false);
  }
}

/* **************************************************************************
 * Demuxer API
 * *************************************************************************/

void HTSPDemuxer::Close0 ( void )
{
  /* Send unsubscribe */
  if (m_subscription.IsActive())
    m_subscription.SendUnsubscribe();

  /* Clear */
  Flush();
  Abort0();
}

void HTSPDemuxer::Abort0 ( void )
{
  CLockObject lock(m_mutex);
  m_streams.clear();
  m_streamStat.clear();
  m_rdsIdx = 0;
  m_seeking = false;
}


bool HTSPDemuxer::Open ( uint32_t channelId, enum eSubscriptionWeight weight )
{
  CLockObject lock(m_conn.Mutex());
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux open");

  /* Close current stream */
  Close0();

  /* Open new subscription */
  m_subscription.SendSubscribe(channelId, weight);
  
  /* Reset status */
  ResetStatus();

  /* Send unsubscribe if subscribing failed */
  if (!m_subscription.IsActive())
    m_subscription.SendUnsubscribe();
  else
    m_lastUse.store(time(nullptr));

  return m_subscription.IsActive();
}

void HTSPDemuxer::Close ( void )
{
  CLockObject lock(m_conn.Mutex());
  Close0();
  ResetStatus();
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux close");
}

DemuxPacket *HTSPDemuxer::Read ( void )
{
  DemuxPacket *pkt = NULL;
  m_lastUse.store(time(nullptr));

  if (m_pktBuffer.Pop(pkt, 100)) {
    Logger::Log(LogLevel::LEVEL_TRACE, "demux read idx :%d pts %lf len %lld",
             pkt->iStreamId, pkt->pts, (long long)pkt->iSize);
    return pkt;
  }
  Logger::Log(LogLevel::LEVEL_TRACE, "demux read nothing");
  
  return PVR->AllocateDemuxPacket(0);
}

void HTSPDemuxer::Flush ( void )
{
  DemuxPacket *pkt;
  Logger::Log(LogLevel::LEVEL_TRACE, "demux flush");
  while (m_pktBuffer.Pop(pkt))
    PVR->FreeDemuxPacket(pkt);
}

void HTSPDemuxer::Trim ( void )
{
  DemuxPacket *pkt;

  Logger::Log(LogLevel::LEVEL_TRACE, "demux trim");
  /* reduce used buffer space to what is needed for DVDPlayer to resume
   * playback without buffering. This depends on the bitrate, so we don't set
   * this too small. */
  while (m_pktBuffer.Size() > 512 && m_pktBuffer.Pop(pkt))
    PVR->FreeDemuxPacket(pkt);
}

void HTSPDemuxer::Abort ( void )
{
  Logger::Log(LogLevel::LEVEL_TRACE, "demux abort");
  CLockObject lock(m_conn.Mutex());
  Abort0();
  ResetStatus();
}

bool HTSPDemuxer::Seek(double time, bool, double *startpts)
{
  if (!m_subscription.IsActive())
    return false;

  m_seekTime = 0;
  m_seeking = true;
  if (!m_subscription.SendSeek(time)) {
    m_seeking = false;
    return false;
  }

  /* Wait for time */
  CLockObject lock(m_conn.Mutex());

  if (!m_seekCond.Wait(m_conn.Mutex(), m_seekTime, Settings::GetInstance().GetResponseTimeout()))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "failed to get subscriptionSeek response");
    m_seeking = false;
    Flush(); /* try to resync */
    return false;
  }
  
  m_seeking = false;
  if (m_seekTime == INVALID_SEEKTIME)
    return false;

  /* Store */
  *startpts = TVH_TO_DVD_TIME(m_seekTime - 1);
  Logger::Log(LogLevel::LEVEL_TRACE, "demux seek startpts = %lf", *startpts);

  return true;
}

void HTSPDemuxer::Speed ( int speed )
{
  CLockObject lock(m_conn.Mutex());
  if (!m_subscription.IsActive())
    return;

  if (speed != 0)
    speed = 1000;

  if (speed != m_subscription.GetSpeed())
    m_subscription.SendSpeed(speed);
}

void HTSPDemuxer::Weight ( enum eSubscriptionWeight weight )
{
  if (!m_subscription.IsActive() || m_subscription.GetWeight() == static_cast<uint32_t>(weight))
    return;
  m_subscription.SendWeight(static_cast<uint32_t>(weight));
}

PVR_ERROR HTSPDemuxer::CurrentStreams ( PVR_STREAM_PROPERTIES *props )
{
  CLockObject lock(m_mutex);

  for (size_t i = 0; i < m_streams.size(); i++)
  {
    memcpy(&props->stream[i], &m_streams.at(i), sizeof(PVR_STREAM_PROPERTIES::PVR_STREAM));
  }

  props->iStreamCount = static_cast<unsigned int>(m_streams.size());
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR HTSPDemuxer::CurrentSignal ( PVR_SIGNAL_STATUS &sig )
{
  CLockObject lock(m_mutex);
  
  sig = { 0 };

  strncpy(sig.strAdapterName,   m_sourceInfo.si_adapter.c_str(),
          sizeof(sig.strAdapterName) - 1);
  strncpy(sig.strAdapterStatus, m_signalInfo.fe_status.c_str(),
          sizeof(sig.strAdapterStatus) - 1);
  strncpy(sig.strServiceName,   m_sourceInfo.si_service.c_str(),
          sizeof(sig.strServiceName) - 1);
  strncpy(sig.strProviderName,  m_sourceInfo.si_provider.c_str(),
          sizeof(sig.strProviderName) - 1);
  strncpy(sig.strMuxName,       m_sourceInfo.si_mux.c_str(),
          sizeof(sig.strMuxName) - 1);
  
  sig.iSNR      = m_signalInfo.fe_snr;
  sig.iSignal   = m_signalInfo.fe_signal;
  sig.iBER      = m_signalInfo.fe_ber;
  sig.iUNC      = m_signalInfo.fe_unc;

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR HTSPDemuxer::CurrentDescrambleInfo ( PVR_DESCRAMBLE_INFO *info )
{
  CLockObject lock(m_mutex);

  *info = { 0 };

  info->iPid = m_descrambleInfo.GetPid();
  info->iCaid = m_descrambleInfo.GetCaid();
  info->iProvid = m_descrambleInfo.GetProvid();
  info->iEcmTime = m_descrambleInfo.GetEcmTime();
  info->iHops = m_descrambleInfo.GetHops();

  strncpy(info->strCardSystem, m_descrambleInfo.GetCardSystem().c_str(), sizeof(info->strCardSystem) - 1);
  strncpy(info->strReader, m_descrambleInfo.GetReader().c_str(), sizeof(info->strReader) - 1);
  strncpy(info->strFrom, m_descrambleInfo.GetFrom().c_str(), sizeof(info->strFrom) - 1);
  strncpy(info->strProtocol, m_descrambleInfo.GetProtocol().c_str(), sizeof(info->strProtocol) - 1);

  return PVR_ERROR_NO_ERROR;
}

bool HTSPDemuxer::IsTimeShifting() const
{
  if (!m_subscription.IsActive())
    return false;

  if (m_subscription.GetSpeed() != SPEED_NORMAL)
    return true;

  CLockObject lock(m_mutex);
  if (m_timeshiftStatus.shift != 0)
    return true;

  return false;
}

bool HTSPDemuxer::IsRealTimeStream() const
{
  return m_subscription.IsActive();
}

PVR_ERROR HTSPDemuxer::GetStreamTimes(PVR_STREAM_TIMES *times) const
{
  *times = {0};

  CLockObject lock(m_mutex);

  times->startTime = m_startTime;
  times->ptsStart = 0;
  times->ptsBegin = TVH_TO_DVD_TIME(m_timeshiftStatus.start);
  times->ptsEnd = TVH_TO_DVD_TIME(m_timeshiftStatus.end);

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

void HTSPDemuxer::SetStreamingProfile(const std::string &profile)
{
  m_subscription.SetProfile(profile);
}

void HTSPDemuxer::ResetStatus(bool resetStartTime /* = true */)
{
  CLockObject lock(m_mutex);

  m_signalInfo.Clear();
  m_sourceInfo.Clear();
  m_descrambleInfo.Clear();
  m_timeshiftStatus.Clear();

  if (resetStartTime)
    m_startTime = 0;
}

/* **************************************************************************
 * Parse incoming data
 * *************************************************************************/

bool HTSPDemuxer::ProcessMessage ( const char *method, htsmsg_t *m )
{
  /* Subscription messages */
  if (!strcmp("muxpkt", method))
    ParseMuxPacket(m);
  else if (!strcmp("subscriptionStatus", method))
    m_subscription.ParseSubscriptionStatus(m);
  else if (!strcmp("queueStatus", method))
    ParseQueueStatus(m);
  else if (!strcmp("signalStatus", method))
    ParseSignalStatus(m);
  else if (!strcmp("timeshiftStatus", method))
    ParseTimeshiftStatus(m);
  else if (!strcmp("descrambleInfo", method))
    ParseDescrambleInfo(m);
  else if (!strcmp("subscriptionStart", method))
    ParseSubscriptionStart(m);
  else if (!strcmp("subscriptionStop", method))
    ParseSubscriptionStop(m);
  else if (!strcmp("subscriptionSkip", method))
    ParseSubscriptionSkip(m);
  else if (!strcmp("subscriptionSpeed", method))
    ParseSubscriptionSpeed(m);
  else if (!strcmp("subscriptionGrace", method))
    ParseSubscriptionGrace(m);
  else
    Logger::Log(LogLevel::LEVEL_DEBUG, "demux unhandled subscription message [%s]", method);

  return true;
}

void HTSPDemuxer::ProcessRDS(uint32_t idx, const void* bin, size_t binlen)
{
  if (idx != m_rdsIdx)
    return;

  const uint8_t* data = static_cast<const uint8_t*>(bin);
  const size_t offset = binlen - 1;

  static const uint8_t RDS_IDENTIFIER = 0xFD;

  if (data[offset] == RDS_IDENTIFIER)
  {
    // RDS data present, obtain length.
    uint8_t rdslen = data[offset - 1];
    if (rdslen > 0)
    {
      const uint32_t rdsIdx = idx - TVH_STREAM_INDEX_OFFSET;
      if (m_streamStat.find(rdsIdx) == m_streamStat.end())
      {
        // No RDS stream yet. Create and announce it.
        if (!AddRDSStream(idx, rdsIdx))
          return;

        // Update streams.
        Logger::Log(LogLevel::LEVEL_DEBUG, "demux stream change");

        DemuxPacket* pktSpecial = PVR->AllocateDemuxPacket(0);
        pktSpecial->iStreamId = DMX_SPECIALID_STREAMCHANGE;
        m_pktBuffer.Push(pktSpecial);
      }

      DemuxPacket* pkt = PVR->AllocateDemuxPacket(rdslen);
      if (!pkt)
        return;

      uint8_t* rdsdata = new uint8_t[rdslen];

      // Reassemble UECP block. mpeg stream contains data in reverse order!
      for (size_t i = offset - 2, j = 0; i > 3 && i > offset - 2 - rdslen; i--, j++)
        rdsdata[j] = data[i];

      memcpy(pkt->pData, rdsdata, rdslen);
      pkt->iSize = rdslen;
      pkt->iStreamId = rdsIdx;

      m_pktBuffer.Push(pkt);
      delete [] rdsdata;
    }
  }
}

void HTSPDemuxer::ParseMuxPacket ( htsmsg_t *m )
{
  uint32_t    idx, u32;
  int64_t     s64;
  const void  *bin;
  size_t      binlen;
  DemuxPacket *pkt;
  char        type = 0;
  int         ignore;

  CLockObject lock(m_mutex);

  /* Ignore packets while switching channels */
  if (!m_subscription.IsActive())
  {
    Logger::Log(LogLevel::LEVEL_DEBUG, "Ignored mux packet due to channel switch");
    return;
  }
  
  /* Validate fields */
  if (htsmsg_get_u32(m, "stream", &idx) ||
      htsmsg_get_bin(m, "payload", &bin, &binlen))
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
  if (!(pkt = PVR->AllocateDemuxPacket(binlen)))
    return;
  memcpy(pkt->pData, bin, binlen);
  pkt->iSize     = binlen;
  pkt->iStreamId = idx;

  /* Duration */
  if (!htsmsg_get_u32(m, "duration", &u32))
    pkt->duration = TVH_TO_DVD_TIME(u32);
  
  /* Timestamps */
  if (!htsmsg_get_s64(m, "dts", &s64))
    pkt->dts      = TVH_TO_DVD_TIME(s64);
  else
    pkt->dts      = DVD_NOPTS_VALUE;

  if (!htsmsg_get_s64(m, "pts", &s64))
    pkt->pts      = TVH_TO_DVD_TIME(s64);
  else
    pkt->pts      = DVD_NOPTS_VALUE;

  /* Type (for debug only) */
  if (!htsmsg_get_u32(m, "frametype", &u32))
    type = (char)u32;
  if (!type)
    type = '_';

  ignore = m_seeking;

  Logger::Log(LogLevel::LEVEL_TRACE, "demux pkt idx %d:%d type %c pts %lf len %lld%s",
           idx, pkt->iStreamId, type, pkt->pts, (long long)binlen,
           ignore ? " IGNORE" : "");

  /* Store */
  if (!ignore)
  {
    if (m_startTime == 0)
    {
      // first paket for this subscription
      m_startTime = time(nullptr);
    }
    m_pktBuffer.Push(pkt);

    // Process RDS data, if present.
    ProcessRDS(idx, bin, binlen);
  }
  else
    PVR->FreeDemuxPacket(pkt);
}

bool HTSPDemuxer::AddRDSStream(uint32_t audioIdx, uint32_t rdsIdx)
{
  for (const auto& stream : m_streams)
  {
    if (stream.iPID != audioIdx)
      continue;

    // Found the stream with the embedded RDS data. Create corresponding RDS stream.
    const CodecDescriptor codecDescriptor = CodecDescriptor::GetCodecByName("rds");
    const xbmc_codec_t codec = codecDescriptor.Codec();

    if (codec.codec_type == XBMC_CODEC_TYPE_UNKNOWN)
      return false;

    m_streamStat[rdsIdx] = 0;

    PVR_STREAM_PROPERTIES::PVR_STREAM rdsStream = {};
    rdsStream.iCodecType = codec.codec_type;
    rdsStream.iCodecId = codec.codec_id;
    rdsStream.iPID = rdsIdx;
    strncpy(rdsStream.strLanguage, stream.strLanguage, sizeof(rdsStream.strLanguage) - 1);

    // We can only use PVR_STREAM_MAX_STREAMS streams
    if (m_streams.size() < PVR_STREAM_MAX_STREAMS)
    {
      Logger::Log(LogLevel::LEVEL_DEBUG, "Adding rds stream. id: %d", rdsIdx);
      m_streams.emplace_back(rdsStream);
      return true;
    }
    else
    {
      Logger::Log(LogLevel::LEVEL_INFO, "Maximum stream limit reached ignoring id: %d, type rds, codec: %u",
                  rdsIdx, rdsStream.iCodecId);
      return false;
    }
  }
  // stream with embedded RDS data not found
  return false;
}

bool HTSPDemuxer::AddTVHStream(uint32_t idx, const char* type, htsmsg_field_t *f)
{
  const CodecDescriptor codecDescriptor = CodecDescriptor::GetCodecByName(type);
  const xbmc_codec_t codec = codecDescriptor.Codec();

  if (codec.codec_type == XBMC_CODEC_TYPE_UNKNOWN)
    return false;

  m_streamStat[idx] = 0;

  PVR_STREAM_PROPERTIES::PVR_STREAM stream = {};
  stream.iCodecType = codec.codec_type;
  stream.iCodecId = codec.codec_id;
  stream.iPID = idx;

  /* Subtitle ID */
  if ((stream.iCodecType == XBMC_CODEC_TYPE_SUBTITLE) &&
      !strcmp("DVBSUB", type))
  {
    uint32_t composition_id = 0, ancillary_id = 0;
    htsmsg_get_u32(&f->hmf_msg, "composition_id", &composition_id);
    htsmsg_get_u32(&f->hmf_msg, "ancillary_id"  , &ancillary_id);
    stream.iSubtitleInfo = (composition_id & 0xffff) | ((ancillary_id & 0xffff) << 16);
  }

  /* Language */
  if (stream.iCodecType == XBMC_CODEC_TYPE_SUBTITLE ||
      stream.iCodecType == XBMC_CODEC_TYPE_AUDIO ||
      stream.iCodecType == XBMC_CODEC_TYPE_RDS)
  {
    const char *language;
    if ((language = htsmsg_get_str(&f->hmf_msg, "language")) != NULL)
      strncpy(stream.strLanguage, language, sizeof(stream.strLanguage) - 1);
  }

  /* Audio data */
  if (stream.iCodecType == XBMC_CODEC_TYPE_AUDIO)
  {
    stream.iChannels = htsmsg_get_u32_or_default(&f->hmf_msg, "channels", 2);
    stream.iSampleRate = htsmsg_get_u32_or_default(&f->hmf_msg, "rate", 48000);

    if (strcmp("MPEG2AUDIO", type) == 0)
    {
      // mpeg2 audio streams may contain embedded RDS data.
      // We will find out when the first stream packet arrives.
      m_rdsIdx = idx;
    }
  }

  /* Video */
  if (stream.iCodecType == XBMC_CODEC_TYPE_VIDEO)
  {
    stream.iWidth  = htsmsg_get_u32_or_default(&f->hmf_msg, "width", 0);
    stream.iHeight = htsmsg_get_u32_or_default(&f->hmf_msg, "height", 0);

    /* Ignore this message if the stream details haven't been determined
       yet, a new message will be sent once they have. This is fixed in
       some versions of tvheadend and is here for backward compatibility. */
    if (stream.iWidth == 0 || stream.iHeight == 0)
    {
      Logger::Log(LogLevel::LEVEL_DEBUG, "Ignoring subscriptionStart, stream details missing");
      return false;
    }

    /* Setting aspect ratio to zero will cause XBMC to handle changes in it */
    stream.fAspect = 0.0f;

    uint32_t duration;
    if ((duration = htsmsg_get_u32_or_default(&f->hmf_msg, "duration", 0)) > 0)
    {
      stream.iFPSScale = duration;
      stream.iFPSRate  = DVD_TIME_BASE;
    }
  }

  /* We can only use PVR_STREAM_MAX_STREAMS streams */
  if (m_streams.size() < PVR_STREAM_MAX_STREAMS)
  {
    Logger::Log(LogLevel::LEVEL_DEBUG, "  id: %d, type %s, codec: %u", idx, type, stream.iCodecId);
    m_streams.emplace_back(stream);
    return true;
  }
  else
  {
    Logger::Log(LogLevel::LEVEL_INFO, "Maximum stream limit reached ignoring id: %d, type %s, codec: %u", idx, type,
                stream.iCodecId);
    return false;
  }
}

void HTSPDemuxer::ParseSubscriptionStart ( htsmsg_t *m )
{
  /* Validate */
  htsmsg_t* l;

  if ((l = htsmsg_get_list(m, "streams")) == NULL)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed subscriptionStart: 'streams' missing");
    return;
  }

  CLockObject lock(m_mutex);

  m_streamStat.clear();
  m_streams.clear();
  m_rdsIdx = 0;

  Logger::Log(LogLevel::LEVEL_DEBUG, "demux subscription start");

  /* Process each */
  htsmsg_field_t* f;
  HTSMSG_FOREACH(f, l)
  {
    if (f->hmf_type != HMF_MAP)
      continue;

    const char *type;
    if ((type = htsmsg_get_str(&f->hmf_msg, "type")) == NULL)
      continue;

    uint32_t idx;
    if (htsmsg_get_u32(&f->hmf_msg, "index", &idx))
      continue;

    idx += TVH_STREAM_INDEX_OFFSET;
    AddTVHStream(idx, type, f);
  }

  /* Update streams */
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux stream change");

  DemuxPacket* pkt = PVR->AllocateDemuxPacket(0);
  pkt->iStreamId = DMX_SPECIALID_STREAMCHANGE;
  m_pktBuffer.Push(pkt);

  /* Source data */
  ParseSourceInfo(htsmsg_get_map(m, "sourceinfo"));
}

void HTSPDemuxer::ParseSourceInfo ( htsmsg_t *m )
{
  const char *str;
  
  /* Ignore */
  if (!m) return;

  Logger::Log(LogLevel::LEVEL_TRACE, "demux sourceInfo:");

  /* include position in mux name
   * as users might receive multiple satellite positions */
  m_sourceInfo.si_mux.clear();
  if ((str = htsmsg_get_str(m, "satpos")) != NULL)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  satpos : %s", str);
    m_sourceInfo.si_mux.append(str);
    m_sourceInfo.si_mux.append(": ");
  }
  if ((str = htsmsg_get_str(m, "mux")) != NULL)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  mux     : %s", str);
    m_sourceInfo.si_mux.append(str);
  }

  if ((str = htsmsg_get_str(m, "adapter")) != NULL)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  adapter : %s", str);
    m_sourceInfo.si_adapter  = str;
  }
  if ((str = htsmsg_get_str(m, "network")) != NULL)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  network : %s", str);
    m_sourceInfo.si_network  = str;
  }
  if ((str = htsmsg_get_str(m, "provider")) != NULL)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  provider : %s", str);
    m_sourceInfo.si_provider = str;
  }
  if ((str = htsmsg_get_str(m, "service")) != NULL)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  service : %s", str);
    m_sourceInfo.si_service  = str;
  }
}

void HTSPDemuxer::ParseSubscriptionStop(htsmsg_t*)
{
  //CLockObject lock(m_mutex);
}

void HTSPDemuxer::ParseSubscriptionSkip ( htsmsg_t *m )
{
  int64_t s64;

  CLockObject lock(m_conn.Mutex());

  if (htsmsg_get_s64(m, "time", &s64)) {
    m_seekTime = INVALID_SEEKTIME;
  } else {
    m_seekTime = s64 < 0 ? 1 : s64 + 1; /* it must not be zero! */
    Flush(); /* flush old packets (with wrong pts) */
  }
  m_seeking = false;
  m_seekCond.Broadcast();
}

void HTSPDemuxer::ParseSubscriptionSpeed ( htsmsg_t *m )
{
  int32_t s32;
  if (!htsmsg_get_s32(m, "speed", &s32))
    Logger::Log(LogLevel::LEVEL_TRACE, "recv speed %d", s32);
}

void HTSPDemuxer::ParseSubscriptionGrace ( htsmsg_t *m )
{
}

void HTSPDemuxer::ParseQueueStatus (htsmsg_t* m)
{
  uint32_t u32;
  std::map<int,int>::const_iterator it;

  CLockObject lock(m_mutex);

  Logger::Log(LogLevel::LEVEL_TRACE, "stream stats:");
  for (it = m_streamStat.begin(); it != m_streamStat.end(); ++it)
    Logger::Log(LogLevel::LEVEL_TRACE, "  idx:%d num:%d", it->first, it->second);

  Logger::Log(LogLevel::LEVEL_TRACE, "queue stats:");
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

void HTSPDemuxer::ParseSignalStatus ( htsmsg_t *m )
{
  uint32_t u32;
  const char *str;

  CLockObject lock(m_mutex);

  /* Reset */
  m_signalInfo.Clear();

  /* Parse */
  Logger::Log(LogLevel::LEVEL_TRACE, "signalStatus:");
  if ((str = htsmsg_get_str(m, "feStatus")) != NULL)
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  status : %s", str);
    m_signalInfo.fe_status = str;
  }
  else
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed signalStatus: 'feStatus' missing, ignoring");
  }
  if (!htsmsg_get_u32(m, "feSNR", &u32))
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  snr    : %d", u32);
    m_signalInfo.fe_snr    = u32;
  }
  if (!htsmsg_get_u32(m, "feBER", &u32))
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  ber    : %d", u32);
    m_signalInfo.fe_ber    = u32;
  }
  if (!htsmsg_get_u32(m, "feUNC", &u32))
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  unc    : %d", u32);
    m_signalInfo.fe_unc    = u32;
  }
  if (!htsmsg_get_u32(m, "feSignal", &u32))
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  signal    : %d", u32);
    m_signalInfo.fe_signal = u32;
  }
}

void HTSPDemuxer::ParseTimeshiftStatus ( htsmsg_t *m )
{
  uint32_t u32;
  int64_t s64;

  CLockObject lock(m_mutex);

  /* Parse */
  Logger::Log(LogLevel::LEVEL_TRACE, "timeshiftStatus:");
  if (!htsmsg_get_u32(m, "full", &u32))
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "  full  : %d", u32);
    m_timeshiftStatus.full = u32 == 0 ? false : true;
  }
  else
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed timeshiftStatus: 'full' missing, ignoring");
  }
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

void HTSPDemuxer::ParseDescrambleInfo(htsmsg_t *m)
{
  uint32_t pid = 0, caid = 0, provid = 0, ecmtime = 0, hops = 0;
  const char *cardsystem, *reader, *from, *protocol;

  /* Parse mandatory fields */
  if (htsmsg_get_u32(m, "pid", &pid) ||
      htsmsg_get_u32(m, "caid", &caid) ||
      htsmsg_get_u32(m, "provid", &provid) ||
      htsmsg_get_u32(m, "ecmtime", &ecmtime) ||
      htsmsg_get_u32(m, "hops", &hops))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed descrambleInfo, mandatory parameters missing");
    return;
  }

  /* Parse optional fields */
  cardsystem = htsmsg_get_str(m, "cardsystem");
  reader = htsmsg_get_str(m, "reader");
  from = htsmsg_get_str(m, "from");
  protocol = htsmsg_get_str(m, "protocol");

  CLockObject lock(m_mutex);

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
