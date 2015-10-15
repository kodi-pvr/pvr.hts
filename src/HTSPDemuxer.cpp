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

#include "Tvheadend.h"
#include "xbmc_codec_descriptor.hpp"

#define TVH_TO_DVD_TIME(x) ((double)x * DVD_TIME_BASE / 1000000.0)

using namespace std;
using namespace ADDON;
using namespace PLATFORM;
using namespace tvheadend;

CHTSPDemuxer::CHTSPDemuxer ( CHTSPConnection &conn )
  : m_conn(conn), m_pktBuffer((size_t)-1),
    m_seekTime(INVALID_SEEKTIME)
{
  m_lastUse = 0;
}

CHTSPDemuxer::~CHTSPDemuxer ()
{
}

void CHTSPDemuxer::Connected ( void )
{
  /* Re-subscribe */
  if (m_subscription.active)
  {
    tvhdebug("demux re-starting stream");
    SendSubscribe(true);
    SendSpeed(true);
  }
}

/* **************************************************************************
 * Demuxer API
 * *************************************************************************/

void CHTSPDemuxer::Close0 ( void )
{
  /* Send unsubscribe */
  if (m_subscription.active)
    SendUnsubscribe();

  /* Clear */
  Flush();
  Abort0();
}

void CHTSPDemuxer::Abort0 ( void )
{
  CLockObject lock(m_mutex);
  m_streams.Clear();
  m_streamStat.clear();
}


bool CHTSPDemuxer::Open ( uint32_t channelId, enum eSubscriptionWeight weight )
{
  CLockObject lock(m_conn.Mutex());
  tvhdebug("demux open");

  /* Close current stream */
  Close0();
  
  /* Create new subscription */
  m_subscription = SSubscription();
  m_subscription.channelId = channelId;
  m_subscription.weight = weight;

  /* Open */
  SendSubscribe();
  
  /* Send unsubscribe if subscribing failed */
  if (!m_subscription.active)
    SendUnsubscribe();
  else
    m_lastUse = time(NULL);
  
  return m_subscription.active;
}

void CHTSPDemuxer::Close ( void )
{
  CLockObject lock(m_conn.Mutex());
  Close0();
  tvhdebug("demux close");
}

DemuxPacket *CHTSPDemuxer::Read ( void )
{
  DemuxPacket *pkt = NULL;
  m_lastUse = time(NULL);
  if (m_pktBuffer.Pop(pkt, 1000)) {
    tvhtrace("demux read idx :%d pts %lf len %lld",
             pkt->iStreamId, pkt->pts, (long long)pkt->iSize);
    return pkt;
  }
  tvhtrace("demux read nothing");
  
  return PVR->AllocateDemuxPacket(0);
}

void CHTSPDemuxer::Flush ( void )
{
  DemuxPacket *pkt;
  tvhtrace("demux flush");
  while (m_pktBuffer.Pop(pkt))
    PVR->FreeDemuxPacket(pkt);
}

void CHTSPDemuxer::Trim ( void )
{
  DemuxPacket *pkt;

  tvhtrace("demux trim");
  /* reduce used buffer space to what is needed for DVDPlayer to resume
   * playback without buffering. This depends on the bitrate, so we don't set
   * this too small. */
  while (m_pktBuffer.Size() > 512 && m_pktBuffer.Pop(pkt))
    PVR->FreeDemuxPacket(pkt);
}

void CHTSPDemuxer::Abort ( void )
{
  tvhtrace("demux abort");
  CLockObject lock(m_conn.Mutex());
  Abort0();
}

bool CHTSPDemuxer::Seek 
  ( int time, bool _unused(backwards), double *startpts )
{
  htsmsg_t *m;

  CLockObject lock(m_conn.Mutex());
  if (!m_subscription.active)
    return false;

  tvhdebug("demux seek %d", time);

  /* Build message */
  m = htsmsg_create_map();  
  htsmsg_add_u32(m, "subscriptionId", m_subscription.subscriptionId);
  htsmsg_add_s64(m, "time",           (int64_t)time * 1000);
  htsmsg_add_u32(m, "absolute",       1);

  /* Send and Wait */
  m = m_conn.SendAndWait("subscriptionSeek", m);
  if (!m)
    return false;
  
  htsmsg_destroy(m);

  /* Wait for time */
  if (!m_seekCond.Wait(m_conn.Mutex(), m_seekTime, Settings::GetInstance().GetResponseTimeout()))
  {
    tvherror("failed to get subscriptionSeek response");
    return false;
  }
  
  if (m_seekTime == INVALID_SEEKTIME)
    return false;

  /* Store */
  *startpts = TVH_TO_DVD_TIME(m_seekTime);
  tvhtrace("demux seek startpts = %lf", *startpts);

  return true;
}

void CHTSPDemuxer::Speed ( int speed )
{
  CLockObject lock(m_conn.Mutex());
  if (!m_subscription.active)
    return;
  m_subscription.speed = speed;
  SendSpeed();
}

void CHTSPDemuxer::Weight ( enum eSubscriptionWeight weight )
{
  if (!m_subscription.active || m_subscription.weight == weight)
    return;
  m_subscription.weight = weight;
  SendWeight();
}

PVR_ERROR CHTSPDemuxer::CurrentStreams ( PVR_STREAM_PROPERTIES *streams )
{
  CLockObject lock(m_mutex);
  return m_streams.GetProperties(streams) ? PVR_ERROR_NO_ERROR
                                          : PVR_ERROR_SERVER_ERROR; 
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

/* **************************************************************************
 * Send Messages
 * *************************************************************************/

void CHTSPDemuxer::SendSubscribe ( bool force )
{
  htsmsg_t *m;

  /* Reset status */
  m_signalInfo.Clear();
  m_sourceInfo.Clear();

  /* Build message */
  m = htsmsg_create_map();
  htsmsg_add_s32(m, "channelId",       m_subscription.channelId);
  htsmsg_add_u32(m, "subscriptionId",  m_subscription.subscriptionId);
  htsmsg_add_u32(m, "weight",          m_subscription.weight);
  htsmsg_add_u32(m, "timeshiftPeriod", (uint32_t)~0);
  htsmsg_add_u32(m, "normts",          1);
  htsmsg_add_u32(m, "queueDepth",      2000000);

  /* Send and Wait for response */
  tvhdebug("demux subscribe to %d", m_subscription.channelId);
  if (force)
    m = m_conn.SendAndWait0("subscribe", m);
  else
    m = m_conn.SendAndWait("subscribe", m);
  if (m == NULL)
    return;

  htsmsg_destroy(m);

  m_subscription.active = true;
  tvhdebug("demux successfully subscribed to %d", m_subscription.channelId);
}

void CHTSPDemuxer::SendUnsubscribe ( void )
{
  htsmsg_t *m;

  /* Build message */
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "subscriptionId", m_subscription.subscriptionId);

  /* Mark subscription as inactive immediately in case this command fails */
  m_subscription.active = false;
  
  /* Send and Wait */
  tvhdebug("demux unsubscribe from %d", m_subscription.channelId);
  if ((m = m_conn.SendAndWait("unsubscribe", m)) == NULL)
    return;

  htsmsg_destroy(m);
  tvhdebug("demux successfully unsubscribed %d", m_subscription.channelId);
}

void CHTSPDemuxer::SendSpeed ( bool force )
{
  htsmsg_t *m;
  int speed = m_subscription.speed / 10; // XBMC uses values an order of magnitude larger than tvheadend

  /* Build message */
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "subscriptionId", m_subscription.subscriptionId);
  htsmsg_add_s32(m, "speed",          speed);
  tvhdebug("demux send speed %d", speed);

  /* Send and Wait */
  if (force)
    m = m_conn.SendAndWait0("subscriptionSpeed", m);
  else
    m = m_conn.SendAndWait("subscriptionSpeed", m);
  if (m)
    htsmsg_destroy(m);
}

void CHTSPDemuxer::SendWeight ( void )
{
  CLockObject lock(m_conn.Mutex());
  htsmsg_t *m;

  /* Build message */
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "subscriptionId", m_subscription.subscriptionId);
  htsmsg_add_s32(m, "weight",         m_subscription.weight);
  tvhdebug("demux send weight %u", m_subscription.weight);

  /* Send and Wait */
  m = m_conn.SendAndWait("subscriptionChangeWeight", m);
  if (m)
    htsmsg_destroy(m);
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
    ParseSubscriptionStatus(m);
  else if (!strcmp("queueStatus", method))
    ParseQueueStatus(m);
  else if (!strcmp("signalStatus", method))
    ParseSignalStatus(m);
  else if (!strcmp("timeshiftStatus", method))
    ParseTimeshiftStatus(m);
  else if (!strcmp("subscriptionStart", method))
    ParseSubscriptionStart(m);
  else if (!strcmp("subscriptionStop", method))
    ParseSubscriptionStop(m);
  else if (!strcmp("subscriptionSkip", method))
    ParseSubscriptionSkip(m);
  else if (!strcmp("subscriptionSpeed", method))
    ParseSubscriptionSpeed(m);
  else
    tvhdebug("demux unhandled subscription message [%s]",
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
  int         iStreamId;
  
  /* Ignore packets while switching channels */
  if (!m_subscription.active)
  {
    tvhdebug("Ignored mux packet due to channel switch");
    return;
  }
  
  /* Validate fields */
  if (htsmsg_get_u32(m, "stream", &idx) ||
      htsmsg_get_bin(m, "payload", &bin, &binlen))
  { 
    tvherror("malformed muxpkt: 'stream'/'payload' missing");
    return;
  }

  /* Record */
  m_streamStat[idx]++;
  
  /* Drop packets for unknown streams */
  if (-1 == (iStreamId = m_streams.GetStreamId(idx)))
  {
    tvhdebug("Dropped packet with unknown stream index %i", idx);
    return;
  }
  
  /* Allocate buffer */
  if (!(pkt = PVR->AllocateDemuxPacket(binlen)))
    return;
  memcpy(pkt->pData, bin, binlen);
  pkt->iSize     = binlen;
  pkt->iStreamId = iStreamId;

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

  tvhtrace("demux pkt idx %d:%d type %c pts %lf len %lld",
           idx, pkt->iStreamId, type, pkt->pts, (long long)binlen);

  /* Store */
  m_pktBuffer.Push(pkt);
}

void CHTSPDemuxer::ParseSubscriptionStart ( htsmsg_t *m )
{
  vector<XbmcPvrStream>  streams;
  htsmsg_t               *l;
  htsmsg_field_t         *f;
  DemuxPacket            *pkt;

  /* Validate */
  if ((l = htsmsg_get_list(m, "streams")) == NULL)
  {
    tvherror("malformed subscriptionStart: 'streams' missing");
    return;
  }
  m_streamStat.clear();

  /* Process each */
  HTSMSG_FOREACH(f, l)
  {
    uint32_t      idx, u32;
    const char    *type;
    XbmcPvrStream stream;

    if (f->hmf_type != HMF_MAP)
      continue;
    if ((type = htsmsg_get_str(&f->hmf_msg, "type")) == NULL)
      continue;
    if (htsmsg_get_u32(&f->hmf_msg, "index", &idx))
      continue;

    /* Find stream */
    m_streamStat[idx] = 0;
    m_streams.GetStreamData(idx, &stream);
    tvhdebug("demux subscription start");
    
    CodecDescriptor codecDescriptor = CodecDescriptor::GetCodecByName(type);
    xbmc_codec_t codec = codecDescriptor.Codec();
    
    if (codec.codec_type != XBMC_CODEC_TYPE_UNKNOWN)
    {
      stream.iCodecType  = codec.codec_type;
      stream.iCodecId    = codec.codec_id;
      stream.iPhysicalId = idx;

      /* Subtitle ID */
      if ((stream.iCodecType == XBMC_CODEC_TYPE_SUBTITLE) &&
          !strcmp("DVBSUB", type))
      {
        uint32_t composition_id = 0, ancillary_id = 0;
        htsmsg_get_u32(&f->hmf_msg, "composition_id", &composition_id);
        htsmsg_get_u32(&f->hmf_msg, "ancillary_id"  , &ancillary_id);
        stream.iIdentifier = (composition_id & 0xffff)
                           | ((ancillary_id & 0xffff) << 16);
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
        stream.iChannels
          = htsmsg_get_u32_or_default(&f->hmf_msg, "channels", 2);
        stream.iSampleRate
          = htsmsg_get_u32_or_default(&f->hmf_msg, "rate", 48000);
      }

      /* Video */
      if (stream.iCodecType == XBMC_CODEC_TYPE_VIDEO)
      {
        stream.iWidth   = htsmsg_get_u32_or_default(&f->hmf_msg, "width", 0);
        stream.iHeight  = htsmsg_get_u32_or_default(&f->hmf_msg, "height", 0);
        
        /* Ignore this message if the stream details haven't been determined 
           yet, a new message will be sent once they have. This is fixed in 
           some versions of tvheadend and is here for backward compatibility. */
        if (stream.iWidth == 0 || stream.iHeight == 0)
        {
          tvhinfo("Ignoring subscriptionStart, stream details missing");
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
        
      streams.push_back(stream);
      tvhdebug("  id: %d, type %s, codec: %u", idx, type, stream.iCodecId);
    }
  }

  /* Update streams */
  tvhdebug("demux stream change");
  m_streams.UpdateStreams(streams);
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
  
  tvhtrace("demux sourceInfo:");

  /* include position in mux name
   * as users might receive multiple satellite positions */
  m_sourceInfo.si_mux.clear();
  if ((str = htsmsg_get_str(m, "satpos")) != NULL)
  {
    tvhtrace("  satpos : %s", str);
    m_sourceInfo.si_mux.append(str);
    m_sourceInfo.si_mux.append(": ");
  }
  if ((str = htsmsg_get_str(m, "mux")) != NULL)
  {
    tvhtrace("  mux     : %s", str);
    m_sourceInfo.si_mux.append(str);
  }

  if ((str = htsmsg_get_str(m, "adapter")) != NULL)
  {
    tvhtrace("  adapter : %s", str);
    m_sourceInfo.si_adapter  = str;
  }
  if ((str = htsmsg_get_str(m, "network")) != NULL)
  {
    tvhtrace("  network : %s", str);
    m_sourceInfo.si_network  = str;
  }
  if ((str = htsmsg_get_str(m, "provider")) != NULL)
  {
    tvhtrace("  provider : %s", str);
    m_sourceInfo.si_provider = str;
  }
  if ((str = htsmsg_get_str(m, "service")) != NULL)
  {
    tvhtrace("  service : %s", str);
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
    m_seekTime = s64;
  }
  m_seekCond.Broadcast();
}

void CHTSPDemuxer::ParseSubscriptionSpeed ( htsmsg_t *m )
{
  uint32_t u32;
  if (!htsmsg_get_u32(m, "speed", &u32))
    tvhtrace("recv speed %d", u32);
}

void CHTSPDemuxer::ParseSubscriptionStatus ( htsmsg_t *m )
{
  const char *status;
  status = htsmsg_get_str(m, "status");

  // not for preTuning and postTuning subscriptions
  if (m_subscription.weight == SUBSCRIPTION_WEIGHT_PRETUNING ||
      m_subscription.weight == SUBSCRIPTION_WEIGHT_POSTTUNING)
    return;

  // this field is absent when everything is fine
  if (status != NULL)
  {
    tvhinfo("Bad subscription status: %s", status);
    XBMC->QueueNotification(QUEUE_INFO, status);
  }
}

void CHTSPDemuxer::ParseQueueStatus ( htsmsg_t *_unused(m) )
{
  uint32_t u32;
  map<int,int>::const_iterator it;
  tvhtrace("stream stats:");
  for (it = m_streamStat.begin(); it != m_streamStat.end(); ++it)
    tvhtrace("  idx:%d num:%d", it->first, it->second);

  tvhtrace("queue stats:");
  if (!htsmsg_get_u32(m, "packets", &u32))
    tvhtrace("  pkts  %d", u32);
  if (!htsmsg_get_u32(m, "bytes", &u32))
    tvhtrace("  bytes %d", u32);
  if (!htsmsg_get_u32(m, "delay", &u32))
    tvhtrace("  delay %d", u32);
  if (!htsmsg_get_u32(m, "Idrops", &u32))
    tvhtrace("  Idrop %d", u32);
  if (!htsmsg_get_u32(m, "Pdrops", &u32))
    tvhtrace("  Pdrop %d", u32);
  if (!htsmsg_get_u32(m, "Bdrops", &u32))
    tvhtrace("  Bdrop %d", u32);
}

void CHTSPDemuxer::ParseSignalStatus ( htsmsg_t *m )
{
  uint32_t u32;
  const char *str;

  /* Reset */
  m_signalInfo.Clear();

  /* Parse */
  tvhtrace("signalStatus:");
  if ((str = htsmsg_get_str(m, "feStatus")) != NULL)
  {
    tvhtrace("  status : %s", str);
    m_signalInfo.fe_status = str;
  }
  else
  {
    tvherror("malformed signalStatus: 'feStatus' missing, ignoring");
  }
  if (!htsmsg_get_u32(m, "feSNR", &u32))
  {
    tvhtrace("  snr    : %d", u32);
    m_signalInfo.fe_snr    = u32;
  }
  if (!htsmsg_get_u32(m, "feBER", &u32))
  {
    tvhtrace("  ber    : %d", u32);
    m_signalInfo.fe_ber    = u32;
  }
  if (!htsmsg_get_u32(m, "feUNC", &u32))
  {
    tvhtrace("  unc    : %d", u32);
    m_signalInfo.fe_unc    = u32;
  }
  if (!htsmsg_get_u32(m, "feSignal", &u32))
  {
    tvhtrace("  signal    : %d", u32);
    m_signalInfo.fe_signal = u32;
  }
}

void CHTSPDemuxer::ParseTimeshiftStatus ( htsmsg_t *m )
{
  uint32_t u32;
  int64_t s64;

  /* Parse */
  tvhtrace("timeshiftStatus:");
  if (!htsmsg_get_u32(m, "full", &u32))
  {
    tvhtrace("  full  : %d", u32);
    m_timeshiftStatus.full = (bool)u32;
  }
  else
  {
    tvherror("malformed timeshiftStatus: 'full' missing, ignoring");
  }
  if (!htsmsg_get_s64(m, "shift", &s64))
  {
    tvhtrace("  shift : %lld", s64);
    m_timeshiftStatus.shift = s64;
  }
  else
  {
    tvherror("malformed timeshiftStatus: 'shift' missing, ignoring");
  }
  if (!htsmsg_get_s64(m, "start", &s64))
  {
    tvhtrace("  start : %lld", s64);
    m_timeshiftStatus.start = s64;
  }
  if (!htsmsg_get_s64(m, "end", &s64))
  {
    tvhtrace("  end   : %lld", s64);
    m_timeshiftStatus.end = s64;
  }
}
