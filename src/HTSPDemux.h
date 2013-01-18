#pragma once

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
#include "platform/util/buffer.h"
#include "platform/threads/mutex.h"

class CHTSPDemux : public CHTSPConnectionCallback
{
public:
  CHTSPDemux(CHTSPConnection* connection);
  ~CHTSPDemux();

  bool         Open(const PVR_CHANNEL &channelinfo);
  void         Close();
  bool         GetStreamProperties(PVR_STREAM_PROPERTIES* props);
  void         Abort();
  DemuxPacket* Read();
  bool         SwitchChannel(const PVR_CHANNEL &channelinfo);
  int         CurrentChannel() { return m_channel; }
  bool         GetSignalStatus(PVR_SIGNAL_STATUS &qualityinfo);
  bool         SeekTime(int time, bool backward, double *startpts);
  void         SetSpeed(int speed);
  bool         OnConnectionRestored(void);
  bool         ProcessMessage(htsmsg* msg);
  void         Flush(void);

private:
  void ParseSubscriptionStart (htsmsg_t *m);
  void ParseSubscriptionStop  (htsmsg_t *m);
  void ParseSubscriptionStatus(htsmsg_t *m);
  void ParseSubscriptionSkip  (htsmsg_t *m);
  void ParseSubscriptionSpeed (htsmsg_t *m);
  bool SendSubscribe  (int subscription, int channel);
  bool SendUnsubscribe(int subscription);
  bool SendSpeed      (int subscription, int speed);
  bool SendSeek       (int subscription, int time, bool backward, double *startpts);
  void ParseMuxPacket(htsmsg_t *m);
  bool ParseQueueStatus(htsmsg_t* msg);
  bool ParseSignalStatus(htsmsg_t* msg);
  bool ParseTimeshiftStatus(htsmsg_t* msg);
  bool ParseSourceInfo(htsmsg_t* msg);

  CHTSPConnection*                     m_session;
  bool                                 m_bIsRadio;
  unsigned                             m_subs;
  int                                  m_channel;
  int                                  m_tag;
  std::string                          m_Status;
  PVR_STREAM_PROPERTIES                m_Streams;
  SChannels                            m_channels;
  SQueueStatus                         m_QueueStatus;
  SQuality                             m_Quality;
  SSourceInfo                          m_SourceInfo;
  std::map<int, unsigned int>          m_StreamIndex;
  PLATFORM::SyncedBuffer<DemuxPacket*> m_demuxPacketBuffer;
  bool                                 m_bIsOpen;
  PLATFORM::CEvent*                    m_seekEvent;
  double                               m_seekTime;
};
