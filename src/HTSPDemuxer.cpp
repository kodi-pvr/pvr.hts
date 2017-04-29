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

#include "tvheadend/utilities/Logger.h"
#include "Tvheadend.h"
#include "xbmc_codec_descriptor.hpp"

#define TVH_TO_DVD_TIME(x) ((double)x * DVD_TIME_BASE / 1000000.0)

using namespace std;
using namespace ADDON;
using namespace P8PLATFORM;
using namespace tvheadend;
using namespace tvheadend::utilities;

CHTSPDemuxer::CHTSPDemuxer ( CHTSPConnection &conn )
  : m_conn(conn), m_pktBuffer((size_t)-1),
    m_seekTime(INVALID_SEEKTIME),
    m_seeking(false), m_speedChange(false),
    m_subscription(conn), m_lastUse(0)
{
}

CHTSPDemuxer::~CHTSPDemuxer ()
{
}

void CHTSPDemuxer::Connected ( void )
{
  /* Re-subscribe */
  if (m_subscription.IsActive())
  {
    Logger::Log(LogLevel::LEVEL_DEBUG, "demux re-starting stream");
    m_subscription.SendSubscribe(0, 0, true);
    m_subscription.SendSpeed(0, true);

    ResetStatus();
  }
}

/* **************************************************************************
 * Demuxer API
 * *************************************************************************/

void CHTSPDemuxer::Close0 ( void )
{
  /* Send unsubscribe */
  if (m_subscription.IsActive())
    m_subscription.SendUnsubscribe();

  /* Clear */
  Flush();
  Abort0();
}

void CHTSPDemuxer::Abort0 ( void )
{
  CLockObject lock(m_mutex);
  m_streams.clear();
  m_streamStat.clear();
  m_seeking = false;
  m_speedChange = false;
}


bool CHTSPDemuxer::Open ( uint32_t channelId, enum eSubscriptionWeight weight )
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

void CHTSPDemuxer::Close ( void )
{
  CLockObject lock(m_conn.Mutex());
  Close0();
  ResetStatus();
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux close");
}

DemuxPacket *CHTSPDemuxer::Read ( void )
{
  DemuxPacket *pkt = NULL;
  m_lastUse.store(time(nullptr));

  if (m_pktBuffer.Pop(pkt, 1000)) {
    Logger::Log(LogLevel::LEVEL_TRACE, "demux read idx :%d pts %lf len %lld",
             pkt->iStreamId, pkt->pts, (long long)pkt->iSize);
    return pkt;
  }
  Logger::Log(LogLevel::LEVEL_TRACE, "demux read nothing");
  
  return PVR->AllocateDemuxPacket(0);
}

void CHTSPDemuxer::Flush ( void )
{
  DemuxPacket *pkt;
  Logger::Log(LogLevel::LEVEL_TRACE, "demux flush");
  while (m_pktBuffer.Pop(pkt))
    PVR->FreeDemuxPacket(pkt);
}

void CHTSPDemuxer::Trim ( void )
{
  DemuxPacket *pkt;

  Logger::Log(LogLevel::LEVEL_TRACE, "demux trim");
  /* reduce used buffer space to what is needed for DVDPlayer to resume
   * playback without buffering. This depends on the bitrate, so we don't set
   * this too small. */
  while (m_pktBuffer.Size() > 512 && m_pktBuffer.Pop(pkt))
    PVR->FreeDemuxPacket(pkt);
}

void CHTSPDemuxer::Abort ( void )
{
  Logger::Log(LogLevel::LEVEL_TRACE, "demux abort");
  CLockObject lock(m_conn.Mutex());
  Abort0();
  ResetStatus();
}

bool CHTSPDemuxer::Seek 
  ( double time, bool _unused(backwards), double *startpts )
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

void CHTSPDemuxer::Speed ( int speed )
{
  CLockObject lock(m_conn.Mutex());
  if (!m_subscription.IsActive())
    return;
  if (speed != m_subscription.GetSpeed() && (speed < 0 || speed >= 4000)) {
    m_speedChange = true;
    Flush();
  }
  m_subscription.SendSpeed(speed);
}

void CHTSPDemuxer::Weight ( enum eSubscriptionWeight weight )
{
  if (!m_subscription.IsActive() || m_subscription.GetWeight() == static_cast<uint32_t>(weight))
    return;
  m_subscription.SendWeight(static_cast<uint32_t>(weight));
}

PVR_ERROR CHTSPDemuxer::CurrentStreams ( PVR_STREAM_PROPERTIES *props )
{
  CLockObject lock(m_mutex);

  for (size_t i = 0; i < m_streams.size(); i++)
  {
    memcpy(&props->stream[i], &m_streams.at(i), sizeof(PVR_STREAM_PROPERTIES::PVR_STREAM));
  }

  props->iStreamCount = static_cast<unsigned int>(m_streams.size());
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CHTSPDemuxer::CurrentSignal ( PVR_SIGNAL_STATUS &sig )
{
  CLockObject lock(m_mutex);
  
  memset(&sig, 0, sizeof(sig));

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

int64_t CHTSPDemuxer::GetTimeshiftTime() const
{
  CLockObject lock(m_mutex);
  return m_timeshiftStatus.shift;
}

int64_t CHTSPDemuxer::GetTimeshiftBufferStart() const
{
  CLockObject lock(m_mutex);

  // Note: start/end mismatch is not a bug. tvh uses inversed naming logic here!
  return m_timeshiftStatus.end;
}

int64_t CHTSPDemuxer::GetTimeshiftBufferEnd() const
{
  CLockObject lock(m_mutex);

  // Note: start/end mismatch is not a bug. tvh uses inversed naming logic here!
  return m_timeshiftStatus.start;
}

bool CHTSPDemuxer::IsTimeShifting() const
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

uint32_t CHTSPDemuxer::GetSubscriptionId() const
{
  return m_subscription.GetId();
}

uint32_t CHTSPDemuxer::GetChannelId() const
{
  if (m_subscription.IsActive())
    return m_subscription.GetChannelId();
  return 0;
}

time_t CHTSPDemuxer::GetLastUse() const
{
  if (m_subscription.IsActive())
    return m_lastUse.load();
  return 0;
}

void CHTSPDemuxer::SetStreamingProfile(const std::string &profile)
{
  m_subscription.SetProfile(profile);
}

bool CHTSPDemuxer::IsRealTimeStream() const
{
  if (!m_subscription.IsActive())
    return false;

  /* Avoid using the getters since they lock individually and
   * we want the calculation to be consistent */
  CLockObject lock(m_mutex);

  /* Handle as real time when reading close to the EOF (10000000�s - 10s) */
  return (m_timeshiftStatus.shift < 10000000);
}

void CHTSPDemuxer::ResetStatus()
{
  CLockObject lock(m_mutex);

  m_signalInfo.Clear();
  m_sourceInfo.Clear();
  m_timeshiftStatus.Clear();
}

/* **************************************************************************
 * Parse incoming data
 * *************************************************************************/

bool CHTSPDemuxer::ProcessMessage ( const char *method, htsmsg_t *m )
{
  CLockObject lock(m_mutex);

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
  else
    Logger::Log(LogLevel::LEVEL_DEBUG, "demux unhandled subscription message [%s]",
              method);

  return true;
}

void CHTSPDemuxer::ParseMuxPacket ( htsmsg_t *m )
{
  uint32_t    idx, u32;
  int64_t     s64;
  const void  *bin;
  size_t      binlen;
  DemuxPacket *pkt;
  char        _unused(type) = 0;
  int         ignore;
  
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

  ignore = m_seeking || m_speedChange;

  Logger::Log(LogLevel::LEVEL_TRACE, "demux pkt idx %d:%d type %c pts %lf len %lld%s",
           idx, pkt->iStreamId, type, pkt->pts, (long long)binlen,
           ignore ? " IGNORE" : "");

  /* Store */
  if (!ignore)
    m_pktBuffer.Push(pkt);
  else
    PVR->FreeDemuxPacket(pkt);
}

void CHTSPDemuxer::ParseSubscriptionStart ( htsmsg_t *m )
{
  htsmsg_t       *l;
  htsmsg_field_t *f;
  DemuxPacket    *pkt;

  /* Validate */
  if ((l = htsmsg_get_list(m, "streams")) == NULL)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed subscriptionStart: 'streams' missing");
    return;
  }
  m_streamStat.clear();
  m_streams.clear();

  Logger::Log(LogLevel::LEVEL_DEBUG, "demux subscription start");

  /* Process each */
  HTSMSG_FOREACH(f, l)
  {
    uint32_t   idx, u32;
    const char *type;

    if (f->hmf_type != HMF_MAP)
      continue;
    if ((type = htsmsg_get_str(&f->hmf_msg, "type")) == NULL)
      continue;
    if (htsmsg_get_u32(&f->hmf_msg, "index", &idx))
      continue;

    /* Find stream */
    m_streamStat[idx] = 0;
    PVR_STREAM_PROPERTIES::PVR_STREAM stream = {};

    Logger::Log(LogLevel::LEVEL_DEBUG, "demux subscription start");
    
    CodecDescriptor codecDescriptor = CodecDescriptor::GetCodecByName(type);
    xbmc_codec_t codec = codecDescriptor.Codec();

    if (codec.codec_type != XBMC_CODEC_TYPE_UNKNOWN)
    {
      stream.iCodecType = codec.codec_type;
      stream.iCodecId   = codec.codec_id;
      stream.iPID       = idx;

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
          stream.iCodecType == XBMC_CODEC_TYPE_AUDIO)
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
          return;
        }
        
        /* Setting aspect ratio to zero will cause XBMC to handle changes in it */
        stream.fAspect = 0.0f;
        
        if ((u32 = htsmsg_get_u32_or_default(&f->hmf_msg, "duration", 0)) > 0)
        {
          stream.iFPSScale = u32;
          stream.iFPSRate  = DVD_TIME_BASE;
        }
      }

      /* We can only use PVR_STREAM_MAX_STREAMS streams */
      if (m_streams.size() < PVR_STREAM_MAX_STREAMS)
      {
        Logger::Log(LogLevel::LEVEL_DEBUG, "  id: %d, type %s, codec: %u", idx, type, stream.iCodecId);
        m_streams.push_back(stream);
      }
      else
      {
        Logger::Log(LogLevel::LEVEL_INFO, "Maximum stream limit reached ignoring id: %d, type %s, codec: %u", idx, type,
                    stream.iCodecId);
      }
    }
  }

  /* Update streams */
  Logger::Log(LogLevel::LEVEL_DEBUG, "demux stream change");
  pkt = PVR->AllocateDemuxPacket(0);
  pkt->iStreamId = DMX_SPECIALID_STREAMCHANGE;
  m_pktBuffer.Push(pkt);

  /* Source data */
  ParseSourceInfo(htsmsg_get_map(m, "sourceinfo"));
}

void CHTSPDemuxer::ParseSourceInfo ( htsmsg_t *m )
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

void CHTSPDemuxer::ParseSubscriptionStop ( htsmsg_t *_unused(m) )
{
}

void CHTSPDemuxer::ParseSubscriptionSkip ( htsmsg_t *m )
{
  CLockObject lock(m_conn.Mutex());
  int64_t s64;
  if (htsmsg_get_s64(m, "time", &s64)) {
    m_seekTime = INVALID_SEEKTIME;
  } else {
    m_seekTime = s64 < 0 ? 1 : s64 + 1; /* it must not be zero! */
    Flush(); /* flush old packets (with wrong pts) */
  }
  m_seeking = false;
  m_seekCond.Broadcast();
}

void CHTSPDemuxer::ParseSubscriptionSpeed ( htsmsg_t *m )
{
  int32_t s32;
  if (!htsmsg_get_s32(m, "speed", &s32))
    Logger::Log(LogLevel::LEVEL_TRACE, "recv speed %d", s32);
  if (m_speedChange) {
    Flush();
    m_speedChange = false;
  }
}

void CHTSPDemuxer::ParseQueueStatus ( htsmsg_t *_unused(m) )
{
  uint32_t u32;
  map<int,int>::const_iterator it;
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

void CHTSPDemuxer::ParseSignalStatus ( htsmsg_t *m )
{
  uint32_t u32;
  const char *str;

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

void CHTSPDemuxer::ParseTimeshiftStatus ( htsmsg_t *m )
{
  uint32_t u32;
  int64_t s64;

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

void CHTSPDemuxer::ParseDescrambleInfo(htsmsg_t *m)
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
