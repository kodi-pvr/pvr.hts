/*
 *      Copyright (C) 2005-2011 Team XBMC
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

#include "client.h"
#include "HTSPConnection.h"
#include "avcodec.h" // For codec id's
#include "HTSPDemux.h"

#define READ_TIMEOUT 20000

using namespace ADDON;
using namespace PLATFORM;

CHTSPDemux::CHTSPDemux(CHTSPConnection* connection) :
    m_session(connection),
    m_bIsRadio(false),
    m_subs(0),
    m_channel(0),
    m_tag(0),
    m_bIsOpen(false)
{
  m_seekEvent = new CEvent;
  m_seekTime  = -1;
  for (unsigned int i = 0; i < PVR_STREAM_MAX_STREAMS; i++)
    m_Streams.stream[i].iCodecType = AVMEDIA_TYPE_UNKNOWN;
  m_Streams.iStreamCount = 0;
  m_StreamIndex.clear();
}

CHTSPDemux::~CHTSPDemux()
{
  Close();
}

bool CHTSPDemux::Open(const PVR_CHANNEL &channelinfo)
{
  m_channel              = channelinfo.iUniqueId;
  m_bIsRadio             = channelinfo.bIsRadio;
  m_bIsOpen              = false;
  m_Streams.iStreamCount = 0;

  if(!m_session->CheckConnection(g_iConnectTimeout * 1000))
    return false;

  if(!SendSubscribe(++m_subs, m_channel))
    return false;

  m_bIsOpen              = true;
  return m_bIsOpen;
}

void CHTSPDemux::Close()
{
  if (m_session->IsConnected() && m_subs > 0)
    SendUnsubscribe(m_subs);
  m_subs = 0;
}

void CHTSPDemux::SetSpeed(int speed)
{
  SendSpeed(m_subs, speed/10);
}

bool CHTSPDemux::SeekTime(int time, bool backward, double *startpts)
{
  return SendSeek(m_subs, time, backward, startpts);
}

bool CHTSPDemux::GetStreamProperties(PVR_STREAM_PROPERTIES* props)
{
  props->iStreamCount = m_Streams.iStreamCount;
  for (unsigned int i = 0; i < m_Streams.iStreamCount; i++)
  {
    props->stream[i].iPhysicalId    = m_Streams.stream[i].iPhysicalId;
    props->stream[i].iCodecType     = m_Streams.stream[i].iCodecType;
    props->stream[i].iCodecId       = m_Streams.stream[i].iCodecId;
    props->stream[i].strLanguage[0] = m_Streams.stream[i].strLanguage[0];
    props->stream[i].strLanguage[1] = m_Streams.stream[i].strLanguage[1];
    props->stream[i].strLanguage[2] = m_Streams.stream[i].strLanguage[2];
    props->stream[i].strLanguage[3] = m_Streams.stream[i].strLanguage[3];
    props->stream[i].iIdentifier    = m_Streams.stream[i].iIdentifier;
    props->stream[i].iFPSScale      = m_Streams.stream[i].iFPSScale;
    props->stream[i].iFPSRate       = m_Streams.stream[i].iFPSRate;
    props->stream[i].iHeight        = m_Streams.stream[i].iHeight;
    props->stream[i].iWidth         = m_Streams.stream[i].iWidth;
    props->stream[i].fAspect        = m_Streams.stream[i].fAspect;
    props->stream[i].iChannels      = m_Streams.stream[i].iChannels;
    props->stream[i].iSampleRate    = m_Streams.stream[i].iSampleRate;
    props->stream[i].iBlockAlign    = m_Streams.stream[i].iBlockAlign;
    props->stream[i].iBitRate       = m_Streams.stream[i].iBitRate;
    props->stream[i].iBitsPerSample = m_Streams.stream[i].iBitsPerSample;
  }
  return (props->iStreamCount > 0);
}

void CHTSPDemux::Abort()
{
  m_Streams.iStreamCount = 0;
  for (unsigned int i = 0; i < PVR_STREAM_MAX_STREAMS; i++)
    m_Streams.stream[i].iCodecType = AVMEDIA_TYPE_UNKNOWN;
  m_StreamIndex.clear();
}

void CHTSPDemux::Flush(void)
{
  DemuxPacket* pkt(NULL);
  while (m_demuxPacketBuffer.Pop(pkt))
    PVR->FreeDemuxPacket(pkt);
}

bool CHTSPDemux::ProcessMessage(htsmsg* msg)
{
  uint32_t subs;
  const char *method = htsmsg_get_str(msg, "method");
  if (!method)
    return true;

  if (    strcmp("subscriptionStart",  method) == 0)
  {
    ParseSubscriptionStart(msg);
  }
  else if(htsmsg_get_u32(msg, "subscriptionId", &subs))
  {
    // no subscription id set, ignore
    return false;
  }
  else if (subs != m_subs)
  {
    // switching channels
    return true;
  }
  else if(strcmp("subscriptionStop",   method) == 0)
    ParseSubscriptionStop(msg);
  else if(strcmp("subscriptionStatus", method) == 0)
    ParseSubscriptionStatus(msg);
  else if(strcmp("subscriptionSkip"  , method) == 0)
    ParseSubscriptionSkip(msg);
  else if(strcmp("subscriptionSpeed" , method) == 0)
    ParseSubscriptionSpeed(msg);
  else if(strcmp("queueStatus"       , method) == 0)
    ParseQueueStatus(msg);
  else if(strcmp("signalStatus"      , method) == 0)
    ParseSignalStatus(msg);
  else if(strcmp("timeshiftStatus"   , method) == 0)
    ParseTimeshiftStatus(msg);
  else if(strcmp("muxpkt"            , method) == 0)
    ParseMuxPacket(msg);
  else
  {
    // not a demux message
    return false;
  }

  return true;
}

DemuxPacket* CHTSPDemux::Read()
{
  if (!m_session->CheckConnection(1000))
    return PVR->AllocateDemuxPacket(0);

  DemuxPacket* packet(NULL);
  if (m_demuxPacketBuffer.Pop(packet, 100))
    return packet;

  return PVR->AllocateDemuxPacket(0);
}

void CHTSPDemux::ParseMuxPacket(htsmsg_t *msg)
{
  uint32_t    index, duration;
  const void* bin;
  size_t      binlen;
  int64_t     ts;

  if(htsmsg_get_u32(msg, "stream" , &index)  ||
     htsmsg_get_bin(msg, "payload", &bin, &binlen))
  {
    XBMC->Log(LOG_ERROR, "%s - malformed message", __FUNCTION__);
    return;
  }

  DemuxPacket* pkt = PVR->AllocateDemuxPacket(binlen);
  if (!pkt)
    return;
  memcpy(pkt->pData, bin, binlen);

  pkt->iSize = binlen;

  if(!htsmsg_get_u32(msg, "duration", &duration))
    pkt->duration = (double)duration * DVD_TIME_BASE / 1000000;

  if(!htsmsg_get_s64(msg, "dts", &ts))
    pkt->dts = (double)ts * DVD_TIME_BASE / 1000000;
  else
    pkt->dts = DVD_NOPTS_VALUE;

  if(!htsmsg_get_s64(msg, "pts", &ts))
    pkt->pts = (double)ts * DVD_TIME_BASE / 1000000;
  else
    pkt->pts = DVD_NOPTS_VALUE;

  pkt->iStreamId = -1;
  for(unsigned int i = 0; i < m_Streams.iStreamCount; i++)
  {
    if(m_Streams.stream[i].iPhysicalId == (unsigned int)index)
    {
      pkt->iStreamId = i;
      break;
    }
  }

  // drop packets with an invalid stream id
  if (pkt->iStreamId < 0)
  {
    PVR->FreeDemuxPacket(pkt);
    return;
  }

  m_demuxPacketBuffer.Push(pkt);
}

bool CHTSPDemux::SwitchChannel(const PVR_CHANNEL &channelinfo)
{
  XBMC->Log(LOG_INFO, "%s - changing to channel '%s'", __FUNCTION__, channelinfo.strChannelName);

  if (!SendUnsubscribe(m_subs))
    XBMC->Log(LOG_ERROR, "%s - failed to unsubscribe from previous channel", __FUNCTION__);

  if (!SendSubscribe(++m_subs, channelinfo.iUniqueId))
  {
    XBMC->Log(LOG_ERROR, "%s - failed to set channel", __FUNCTION__);
    m_subs = 0;
  }
  else
  {
    m_channel              = channelinfo.iUniqueId;
    m_Streams.iStreamCount = 0;

    return true;
  }
  return false;
}

bool CHTSPDemux::GetSignalStatus(PVR_SIGNAL_STATUS &qualityinfo)
{
  memset(&qualityinfo, 0, sizeof(qualityinfo));
  if (m_SourceInfo.si_adapter.empty() || m_Quality.fe_status.empty())
    return false;

  strncpy(qualityinfo.strAdapterName, m_SourceInfo.si_adapter.c_str(), sizeof(qualityinfo.strAdapterName));
  strncpy(qualityinfo.strAdapterStatus, m_Quality.fe_status.c_str(), sizeof(qualityinfo.strAdapterStatus));
  qualityinfo.iSignal       = (uint16_t)m_Quality.fe_signal;
  qualityinfo.iSNR          = (uint16_t)m_Quality.fe_snr;
  qualityinfo.iBER          = (uint32_t)m_Quality.fe_ber;
  qualityinfo.iUNC          = (uint32_t)m_Quality.fe_unc;

  return true;
}

inline void HTSPResetDemuxStreamInfo(PVR_STREAM_PROPERTIES::PVR_STREAM &stream)
{
  memset(&stream, 0, sizeof(stream));
  stream.iIdentifier    = -1;
}

inline void HTSPSetDemuxStreamInfoAudio(PVR_STREAM_PROPERTIES::PVR_STREAM &stream, htsmsg_t *msg)
{
  stream.iChannels   = htsmsg_get_u32_or_default(msg, "channels" , 0);
  stream.iSampleRate = htsmsg_get_u32_or_default(msg, "rate" , 0);
}

inline void HTSPSetDemuxStreamInfoVideo(PVR_STREAM_PROPERTIES::PVR_STREAM &stream, htsmsg_t *msg)
{
  stream.iWidth  = htsmsg_get_u32_or_default(msg,   "width" , 0);
  stream.iHeight = htsmsg_get_u32_or_default(msg,   "height" , 0);
  uint32_t den   = htsmsg_get_u32_or_default(msg, "aspect_den", 1);
  if(den)
    stream.fAspect = (float)htsmsg_get_u32_or_default(msg, "aspect_num", 1) / den;
  else
    stream.fAspect = 0.0f;
  uint32_t iDuration = htsmsg_get_u32_or_default(msg, "duration" , 0);
  if (iDuration > 0)
  {
    stream.iFPSScale = iDuration;
    stream.iFPSRate  = DVD_TIME_BASE;
  }
}

inline void HTSPSetDemuxStreamInfoLanguage(PVR_STREAM_PROPERTIES::PVR_STREAM &stream, htsmsg_t *msg)
{
  if (const char *strLanguage = htsmsg_get_str(msg, "language"))
  {
    stream.strLanguage[0] = strLanguage[0];
    stream.strLanguage[1] = strLanguage[1];
    stream.strLanguage[2] = strLanguage[2];
    stream.strLanguage[3] = 0;
  }
}

void CHTSPDemux::ParseSubscriptionStart(htsmsg_t *m)
{
  PVR_STREAM_PROPERTIES newStreams;
  newStreams.iStreamCount = 0;
  std::map<int, unsigned int> newStreamIndex;

  htsmsg_t       *streams;
  htsmsg_field_t *f;
  uint32_t        subs;

  if(htsmsg_get_u32(m, "subscriptionId", &subs))
  {
    XBMC->Log(LOG_ERROR, "%s - invalid subscription id", __FUNCTION__);
    return;
  }
  m_subs = subs;

  if((streams = htsmsg_get_list(m, "streams")) == NULL)
  {
    XBMC->Log(LOG_ERROR, "%s - malformed message", __FUNCTION__);
    return;
  }

  m_Streams.iStreamCount = 0;

  HTSMSG_FOREACH(f, streams)
  {
    uint32_t    index;
    const char* type;
    htsmsg_t*   sub;

    if (newStreams.iStreamCount >= PVR_STREAM_MAX_STREAMS)
    {
      XBMC->Log(LOG_ERROR, "%s - max amount of streams reached", __FUNCTION__);
      break;
    }

    if (f->hmf_type != HMF_MAP)
      continue;

    sub = &f->hmf_msg;

    if ((type = htsmsg_get_str(sub, "type")) == NULL)
      continue;

    if (htsmsg_get_u32(sub, "index", &index))
      continue;

    XBMC->Log(LOG_DEBUG, "%s - id: %d, type: %s", __FUNCTION__, index, type);

    bool bValidStream(true);
    HTSPResetDemuxStreamInfo(newStreams.stream[newStreams.iStreamCount]);

    std::map<int,unsigned int>::iterator it = m_StreamIndex.find(index);
    if (it != m_StreamIndex.end())
    {
      memcpy((void*)&newStreams.stream[newStreams.iStreamCount], (void*)&m_Streams.stream[m_StreamIndex[index]],
          sizeof(PVR_STREAM_PROPERTIES::PVR_STREAM));
    }

    if(!strcmp(type, "AC3"))
    {
      newStreams.stream[newStreams.iStreamCount].iCodecType = AVMEDIA_TYPE_AUDIO;
      newStreams.stream[newStreams.iStreamCount].iCodecId   = CODEC_ID_AC3;
    }
    else if(!strcmp(type, "EAC3"))
    {
      newStreams.stream[newStreams.iStreamCount].iCodecType = AVMEDIA_TYPE_AUDIO;
      newStreams.stream[newStreams.iStreamCount].iCodecId   = CODEC_ID_EAC3;
    }
    else if(!strcmp(type, "MPEG2AUDIO"))
    {
      newStreams.stream[newStreams.iStreamCount].iCodecType = AVMEDIA_TYPE_AUDIO;
      newStreams.stream[newStreams.iStreamCount].iCodecId   = CODEC_ID_MP2;
    }
    else if(!strcmp(type, "AAC"))
    {
      newStreams.stream[newStreams.iStreamCount].iCodecType = AVMEDIA_TYPE_AUDIO;
      newStreams.stream[newStreams.iStreamCount].iCodecId   = CODEC_ID_AAC;
    }
    else if(!strcmp(type, "AACLATM"))
    {
      newStreams.stream[newStreams.iStreamCount].iCodecType  = AVMEDIA_TYPE_AUDIO;
      newStreams.stream[newStreams.iStreamCount].iCodecId    = CODEC_ID_AAC_LATM;
    }
    else if(!strcmp(type, "VORBIS"))
    {
      newStreams.stream[newStreams.iStreamCount].iCodecType  = AVMEDIA_TYPE_AUDIO;
      newStreams.stream[newStreams.iStreamCount].iCodecId    = CODEC_ID_VORBIS;
    }
    else if(!strcmp(type, "MPEG2VIDEO"))
    {
      newStreams.stream[newStreams.iStreamCount].iCodecType = AVMEDIA_TYPE_VIDEO;
      newStreams.stream[newStreams.iStreamCount].iCodecId   = CODEC_ID_MPEG2VIDEO;
    }
    else if(!strcmp(type, "H264"))
    {
      newStreams.stream[newStreams.iStreamCount].iCodecType = AVMEDIA_TYPE_VIDEO;
      newStreams.stream[newStreams.iStreamCount].iCodecId   = CODEC_ID_H264;
    }
    else if(!strcmp(type, "VP8"))
    {
      newStreams.stream[newStreams.iStreamCount].iCodecType = AVMEDIA_TYPE_VIDEO;
      newStreams.stream[newStreams.iStreamCount].iCodecId   = CODEC_ID_VP8;
    }
    else if(!strcmp(type, "MPEG4VIDEO"))
    {
      newStreams.stream[newStreams.iStreamCount].iCodecType = AVMEDIA_TYPE_VIDEO;
      newStreams.stream[newStreams.iStreamCount].iCodecId   = CODEC_ID_MPEG4;
    }
    else if(!strcmp(type, "DVBSUB"))
    {
      uint32_t composition_id = 0, ancillary_id = 0;
      htsmsg_get_u32(sub, "composition_id", &composition_id);
      htsmsg_get_u32(sub, "ancillary_id"  , &ancillary_id);

      newStreams.stream[newStreams.iStreamCount].iCodecType  = AVMEDIA_TYPE_SUBTITLE;
      newStreams.stream[newStreams.iStreamCount].iCodecId    = CODEC_ID_DVB_SUBTITLE;
      newStreams.stream[newStreams.iStreamCount].iIdentifier = (composition_id & 0xffff) | ((ancillary_id & 0xffff) << 16);
      HTSPSetDemuxStreamInfoLanguage(newStreams.stream[newStreams.iStreamCount], sub);
    }
    else if(!strcmp(type, "TEXTSUB"))
    {
      newStreams.stream[newStreams.iStreamCount].iCodecType = AVMEDIA_TYPE_SUBTITLE;
      newStreams.stream[newStreams.iStreamCount].iCodecId   = CODEC_ID_TEXT;
      HTSPSetDemuxStreamInfoLanguage(newStreams.stream[newStreams.iStreamCount], sub);
    }
    else if(!strcmp(type, "TELETEXT"))
    {
      newStreams.stream[newStreams.iStreamCount].iCodecType = AVMEDIA_TYPE_SUBTITLE;
      newStreams.stream[newStreams.iStreamCount].iCodecId   = CODEC_ID_DVB_TELETEXT;
    }
    else
    {
      bValidStream = false;
    }

    if (bValidStream)
    {
      newStreamIndex[index] = newStreams.iStreamCount;
      newStreams.stream[newStreams.iStreamCount].iPhysicalId  = index;
      if (newStreams.stream[newStreams.iStreamCount].iCodecType == AVMEDIA_TYPE_AUDIO)
      {
        HTSPSetDemuxStreamInfoAudio(newStreams.stream[newStreams.iStreamCount], sub);
        HTSPSetDemuxStreamInfoLanguage(newStreams.stream[newStreams.iStreamCount], sub);
      }
      else if (newStreams.stream[newStreams.iStreamCount].iCodecType == AVMEDIA_TYPE_VIDEO)
        HTSPSetDemuxStreamInfoVideo(newStreams.stream[newStreams.iStreamCount], sub);
      ++newStreams.iStreamCount;
    }
  }

  std::map<int,unsigned int>::iterator itl, itr;
  // delete streams we don't have in streams
  itl = m_StreamIndex.begin();
  while (itl != m_StreamIndex.end())
  {
    itr = newStreamIndex.find(itl->first);
    if (itr == newStreamIndex.end())
    {
      m_Streams.stream[itl->second].iCodecType = AVMEDIA_TYPE_UNKNOWN;
      m_Streams.stream[itl->second].iCodecId   = CODEC_ID_NONE;
      m_StreamIndex.erase(itl);
      itl = m_StreamIndex.begin();
    }
    else
      ++itl;
  }
  // copy known streams
  for (itl = m_StreamIndex.begin(); itl != m_StreamIndex.end(); ++itl)
  {
    itr = newStreamIndex.find(itl->first);
    memcpy((void*)&m_Streams.stream[itl->second], (void*)&newStreams.stream[itr->second],
              sizeof(PVR_STREAM_PROPERTIES::PVR_STREAM));
    newStreamIndex.erase(itr);
  }

  // place video stream at pos 0
  for (itr = newStreamIndex.begin(); itr != newStreamIndex.end(); ++itr)
  {
    if (newStreams.stream[itr->second].iCodecType == AVMEDIA_TYPE_VIDEO)
      break;
  }
  if (itr != newStreamIndex.end())
  {
    m_StreamIndex[itr->first] = 0;
    memcpy((void*)&m_Streams.stream[0], (void*)&newStreams.stream[itr->second],
              sizeof(PVR_STREAM_PROPERTIES::PVR_STREAM));
    newStreamIndex.erase(itr);
  }

  // fill the gaps or append after highest index
  while (!newStreamIndex.empty())
  {
    // find first unused index
    unsigned int i;
    for (i = 0; i < PVR_STREAM_MAX_STREAMS; i++)
    {
      if (m_Streams.stream[i].iCodecType == (unsigned)AVMEDIA_TYPE_UNKNOWN)
        break;
    }
    itr = newStreamIndex.begin();
    m_StreamIndex[itr->first] = i;
    memcpy((void*)&m_Streams.stream[i], (void*)&newStreams.stream[itr->second],
              sizeof(PVR_STREAM_PROPERTIES::PVR_STREAM));
    newStreamIndex.erase(itr);
  }

  // set streamCount
  m_Streams.iStreamCount = 0;
  for (itl = m_StreamIndex.begin(); itl != m_StreamIndex.end(); ++itl)
  {
    if (itl->second > m_Streams.iStreamCount)
      m_Streams.iStreamCount = itl->second;
  }
  if (!m_StreamIndex.empty())
    m_Streams.iStreamCount++;

  DemuxPacket* pkt  = PVR->AllocateDemuxPacket(0);
  pkt->iStreamId    = DMX_SPECIALID_STREAMCHANGE;
  m_demuxPacketBuffer.Push(pkt);

  if (ParseSourceInfo(m))
  {
    XBMC->Log(LOG_INFO, "%s - subscription started on adapter %s, mux %s, network %s, provider %s, service %s"
        , __FUNCTION__, m_SourceInfo.si_adapter.c_str(), m_SourceInfo.si_mux.c_str(),
        m_SourceInfo.si_network.c_str(), m_SourceInfo.si_provider.c_str(),
        m_SourceInfo.si_service.c_str());
  }
  else
  {
    XBMC->Log(LOG_INFO, "%s - subscription started on an unknown device", __FUNCTION__);
  }
}

void CHTSPDemux::ParseSubscriptionStop(htsmsg_t *m)
{
  XBMC->Log(LOG_INFO, "%s - subscription ended on adapter %s", __FUNCTION__, m_SourceInfo.si_adapter.c_str());
  m_Streams.iStreamCount = 0;

  /* reset the signal status */
  m_Quality.fe_status = "";
  m_Quality.fe_ber    = -2;
  m_Quality.fe_signal = -2;
  m_Quality.fe_snr    = -2;
  m_Quality.fe_unc    = -2;

  /* reset the source info */
  m_SourceInfo.si_adapter  = "";
  m_SourceInfo.si_mux      = "";
  m_SourceInfo.si_network  = "";
  m_SourceInfo.si_provider = "";
  m_SourceInfo.si_service  = "";
}

void CHTSPDemux::ParseSubscriptionStatus(htsmsg_t *m)
{
  const char* status;
  status = htsmsg_get_str(m, "status");
  if(status == NULL)
    m_Status = "";
  else
  {
    m_Status = status;
    XBMC->Log(LOG_INFO, "%s - status = '%s'", __FUNCTION__, status);
    XBMC->QueueNotification(QUEUE_INFO, status);
  }
}

void CHTSPDemux::ParseSubscriptionSkip(htsmsg_t *m)
{
  int64_t s64;
  uint32_t u32;
  if (!htsmsg_get_u32(m, "error", &u32)   ||
       htsmsg_get_u32(m, "absolute", &u32) ||
       htsmsg_get_s64(m, "time", &s64)) {
    m_seekTime = -1;
  } else {
    m_seekTime = (double)s64;
  }
  XBMC->Log(LOG_DEBUG, "HTSP::ParseSubscriptionSkip - skip = %lf\n", m_seekTime);
  m_seekEvent->Broadcast();
}

void CHTSPDemux::ParseSubscriptionSpeed(htsmsg_t *m)
{
  uint32_t u32;
  if (!htsmsg_get_u32(m, "speed", &u32)) {
    XBMC->Log(LOG_INFO, "%s - speed = %u", __FUNCTION__, u32);
    // TODO: need a way to pass this to player core
  }
}

bool CHTSPDemux::SendUnsubscribe(int subscription)
{
  XBMC->Log(LOG_INFO, "%s - unsubscribe from subscription %d", __FUNCTION__, subscription);

  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "method"        , "unsubscribe");
  htsmsg_add_s32(m, "subscriptionId", subscription);
  bool bReturn = m_session->ReadSuccess(m, "unsubscribe from channel");
  m_session->SetReadTimeout(-1);
  Flush();
  m_bIsOpen = false;
  return bReturn;
}

bool CHTSPDemux::SendSubscribe(int subscription, int channel)
{
  const char *audioCodec;
  const char *videoCodec;

  XBMC->Log(LOG_INFO, "%s - subscribe to channel '%d', subscription %d", __FUNCTION__, channel, subscription);

  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "method"         , "subscribe");
  htsmsg_add_s32(m, "channelId"      , channel);
  htsmsg_add_s32(m, "subscriptionId" , subscription);
  htsmsg_add_u32(m, "timeshiftPeriod", (uint32_t)~0);

  if(g_bTranscode)
  {
    switch(g_iAudioCodec)
    {
      case CODEC_ID_MP2:
        audioCodec = "MPEG2AUDIO";
        break;
      case CODEC_ID_AAC:
        audioCodec = "AAC";
        break;
      case CODEC_ID_AC3:
        audioCodec = "AC3";
        break;
      case CODEC_ID_VORBIS:
        audioCodec = "VORBIS";
        break;
      default:
        audioCodec = "UNKNOWN";
        break;
    }

    switch(g_iVideoCodec)
    {
      case CODEC_ID_MPEG2VIDEO:
        videoCodec = "MPEG2VIDEO";
        break;
      case CODEC_ID_H264:
        videoCodec = "H264";
        break;
      case CODEC_ID_VP8:
        videoCodec = "VP8";
        break;
      case CODEC_ID_MPEG4:
        videoCodec = "MPEG4VIDEO";
        break;
      default:
        videoCodec = "UNKNOWN";
        break;
    }

    htsmsg_add_u32(m, "maxResolution", g_iResolution);
    htsmsg_add_str(m, "audioCodec"   , audioCodec);
    htsmsg_add_str(m, "videoCodec"   , videoCodec);
  }

  if (!m_session->ReadSuccess(m, "subscribe to channel"))
  {
    XBMC->Log(LOG_ERROR, "%s - failed to subscribe to channel %d, consider the connection dropped", __FUNCTION__, m_channel);
    m_session->TriggerReconnect();
    return false;
  }

  // TODO get this from the pvr api. hardcoded to 10 seconds now
  m_session->SetReadTimeout(READ_TIMEOUT);
  Flush();

  XBMC->Log(LOG_DEBUG, "%s - new subscription for channel %d (%d)", __FUNCTION__, m_channel, m_subs);
  return true;
}

bool CHTSPDemux::SendSpeed(int subscription, int speed)
{
  XBMC->Log(LOG_DEBUG, "%s(%d, %d)", __FUNCTION__, subscription, speed);
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "method"        , "subscriptionSpeed");
  htsmsg_add_s32(m, "subscriptionId", subscription);
  htsmsg_add_s32(m, "speed"         , speed);
  if (m_session->ReadSuccess(m, "pause subscription"))
  {
    m_session->SetReadTimeout(speed == 0 ? -1 : READ_TIMEOUT);
    return true;
  }
  return false;
}

bool CHTSPDemux::SendSeek(int subscription, int time, bool backward, double *startpts)
{
  htsmsg_t *m = htsmsg_create_map();
  int64_t seek;

  // Note: time is in MSEC not DVD_TIME_BASE, TVH requires 1MHz (us) input
  seek = time * 1000;
  XBMC->Log(LOG_DEBUG, "%s(time=%d, seek=%ld)", __FUNCTION__, time, seek);

  htsmsg_add_str(m, "method"        , "subscriptionSkip");
  htsmsg_add_s32(m, "subscriptionId", subscription);
  htsmsg_add_s64(m, "time"          , seek);
  htsmsg_add_u32(m, "absolute"      , 1);

  if (!m_session->ReadSuccess(m, "seek subscription"))
    return false;

  if (!m_seekEvent->Wait(g_iResponseTimeout * 1000))
    return false;

  if (m_seekTime < 0)
    return false;

  // Note: return value is in DVD_TIME_BASE not MSEC
  *startpts = m_seekTime * DVD_TIME_BASE / 1000000;
  XBMC->Log(LOG_DEBUG, "%s(%ld) = %lf", __FUNCTION__, seek, *startpts);
  return true;
}

bool CHTSPDemux::ParseQueueStatus(htsmsg_t* msg)
{
  if(htsmsg_get_u32(msg, "packets", &m_QueueStatus.packets)
  || htsmsg_get_u32(msg, "bytes",   &m_QueueStatus.bytes)
  || htsmsg_get_u32(msg, "Bdrops",  &m_QueueStatus.bdrops)
  || htsmsg_get_u32(msg, "Pdrops",  &m_QueueStatus.pdrops)
  || htsmsg_get_u32(msg, "Idrops",  &m_QueueStatus.idrops))
  {
    XBMC->Log(LOG_ERROR, "%s - malformed message received", __FUNCTION__);
    htsmsg_print(msg);
    return false;
  }

  /* delay isn't always transmitted */
  if(htsmsg_get_u32(msg, "delay", &m_QueueStatus.delay))
    m_QueueStatus.delay = 0;

  return true;
}

bool CHTSPDemux::ParseSignalStatus(htsmsg_t* msg)
{
  if(htsmsg_get_u32(msg, "feSNR",    &m_Quality.fe_snr))
    m_Quality.fe_snr = -2;

  if(htsmsg_get_u32(msg, "feSignal", &m_Quality.fe_signal))
    m_Quality.fe_signal = -2;

  if(htsmsg_get_u32(msg, "feBER",    &m_Quality.fe_ber))
    m_Quality.fe_ber = -2;

  if(htsmsg_get_u32(msg, "feUNC",    &m_Quality.fe_unc))
    m_Quality.fe_unc = -2;

  const char* status;
  if((status = htsmsg_get_str(msg, "feStatus")))
    m_Quality.fe_status = status;
  else
    m_Quality.fe_status = "(unknown)";

  return true;
}

bool CHTSPDemux::ParseTimeshiftStatus(htsmsg_t *msg)
{
  // TODO: placeholder for processing timeshiftStatus message when
  //       we're ready to use the information.
  //
  //       For now this just ensures we don't spam logs with unecessary
  //       info about unhandled messages.
  return true;
}

bool CHTSPDemux::ParseSourceInfo(htsmsg_t* msg)
{
  htsmsg_t       *sourceinfo;
  if((sourceinfo = htsmsg_get_map(msg, "sourceinfo")) == NULL)
  {
    XBMC->Log(LOG_ERROR, "%s - malformed message", __FUNCTION__);
    return false;
  }

  const char* str;
  if((str = htsmsg_get_str(sourceinfo, "adapter")) == NULL)
    m_SourceInfo.si_adapter = "";
  else
    m_SourceInfo.si_adapter = str;

  if((str = htsmsg_get_str(sourceinfo, "mux")) == NULL)
    m_SourceInfo.si_mux = "";
  else
    m_SourceInfo.si_mux = str;

  if((str = htsmsg_get_str(sourceinfo, "network")) == NULL)
    m_SourceInfo.si_network = "";
  else
    m_SourceInfo.si_network = str;

  if((str = htsmsg_get_str(sourceinfo, "provider")) == NULL)
    m_SourceInfo.si_provider = "";
  else
    m_SourceInfo.si_provider = str;

  if((str = htsmsg_get_str(sourceinfo, "service")) == NULL)
    m_SourceInfo.si_service = "";
  else
    m_SourceInfo.si_service = str;

  return true;
}

bool CHTSPDemux::OnConnectionRestored(void)
{
  if (m_subs == 0)
    return true;

  SendUnsubscribe(m_subs);

  if (!SendSubscribe(++m_subs, m_channel))
  {
    m_subs = 0;
    XBMC->Log(LOG_ERROR, "%s - failed to subscribe to channel %d", __FUNCTION__, m_channel);
    return false;
  }

  return true;
}
