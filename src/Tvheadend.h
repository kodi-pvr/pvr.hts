#pragma once

/*
 *      Copyright (C) 2005-2014 Team XBMC
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

extern "C" {
#include <sys/types.h>
#include "libhts/htsmsg.h"
}

#include <utility>
#include <string>
#include <vector>

#include "p8-platform/util/buffer.h"
#include "p8-platform/threads/threads.h"

#include "tvheadend/AutoRecordings.h"
#include "tvheadend/ChannelTuningPredictor.h"
#include "tvheadend/HTSPMessage.h"
#include "tvheadend/IHTSPConnectionListener.h"
#include "tvheadend/Profile.h"
#include "tvheadend/TimeRecordings.h"
#include "tvheadend/entity/Channel.h"
#include "tvheadend/entity/Recording.h"
#include "tvheadend/entity/Schedule.h"
#include "tvheadend/entity/Tag.h"
#include "tvheadend/utilities/AsyncState.h"
#include "client.h"

#define UNNUMBERED_CHANNEL (10000)

/*
 * Forward decleration of classes
 */
namespace tvheadend
{
class HTSPConnection;
class HTSPDemuxer;
class HTSPVFS;
}

/* Typedefs */
typedef P8PLATFORM::SyncedBuffer<tvheadend::HTSPMessage> HTSPMessageQueue;

/*
 * Root object for Tvheadend connection
 */
class CTvheadend
  : public P8PLATFORM::CThread, public tvheadend::IHTSPConnectionListener
{
public:
  CTvheadend(PVR_PROPERTIES *pvrProps);
  ~CTvheadend() override;

  void Start ( void );
  void Stop ( void );

  // IHTSPConnectionListener implementation
  void Disconnected() override;
  bool Connected() override;
  bool ProcessMessage(const char *method, htsmsg_t *msg) override;

  const tvheadend::entity::Channels& GetChannels () const
  {
    return m_channels;
  }

  PVR_ERROR GetDriveSpace     ( long long *total, long long *used );

  int       GetTagCount       ( void );
  PVR_ERROR GetTags           ( ADDON_HANDLE handle, bool bRadio );
  PVR_ERROR GetTagMembers     ( ADDON_HANDLE handle,
                                const PVR_CHANNEL_GROUP &group );

  int       GetChannelCount   ( void );
  PVR_ERROR GetChannels       ( ADDON_HANDLE handle, bool radio );

  int       GetRecordingCount ( void );
  PVR_ERROR GetRecordings     ( ADDON_HANDLE handle );
  PVR_ERROR GetRecordingEdl   ( const PVR_RECORDING &rec, PVR_EDL_ENTRY edl[],
                                int *num );
  PVR_ERROR DeleteRecording   ( const PVR_RECORDING &rec );
  PVR_ERROR RenameRecording   ( const PVR_RECORDING &rec );
  PVR_ERROR SetLifetime       (const PVR_RECORDING &rec);
  PVR_ERROR SetPlayCount      ( const PVR_RECORDING &rec, int playcount );
  PVR_ERROR SetPlayPosition   ( const PVR_RECORDING &rec, int playposition );
  int       GetPlayPosition   ( const PVR_RECORDING &rec );
  PVR_ERROR GetTimerTypes     ( PVR_TIMER_TYPE types[], int *size );
  int       GetTimerCount     ( void );
  PVR_ERROR GetTimers         ( ADDON_HANDLE handle );
  PVR_ERROR AddTimer          ( const PVR_TIMER &tmr );
  PVR_ERROR DeleteTimer       ( const PVR_TIMER &tmr, bool force );
  PVR_ERROR UpdateTimer       ( const PVR_TIMER &tmr );

  PVR_ERROR GetEPGForChannel  ( ADDON_HANDLE handle, const PVR_CHANNEL &chn,
                                time_t start, time_t end );
  PVR_ERROR SetEPGTimeFrame   ( int iDays );

  void GetLivetimeValues(std::vector<std::pair<int, std::string>>& lifetimeValues) const;

private:
  bool      CreateTimer       ( const tvheadend::entity::Recording &tvhTmr, PVR_TIMER &tmr );

  uint32_t GetNextUnnumberedChannelNumber ();
  std::string GetImageURL     ( const char *str );

  /**
   * Queries the server for available streaming profiles and populates
   * m_profiles
   */
  void QueryAvailableProfiles();

  /**
   * @param streamingProfile the streaming profile to check for
   * @return whether the server supports the specified streaming profile
   */
  bool HasStreamingProfile(const std::string &streamingProfile) const;

  /*
   * Predictive tuning
   */
  void PredictiveTune         ( uint32_t fromChannelId, uint32_t toChannelId );
  void TuneOnOldest           ( uint32_t channelId );

  /*
   * Message processing (CThread implementation)
   */
  void *Process() override;

  /*
   * Event handling
   */
  void TriggerChannelGroupsUpdate();
  void TriggerChannelUpdate();
  void TriggerRecordingUpdate();
  void TriggerTimerUpdate();
  void PushEpgEventUpdate(const tvheadend::entity::Event &epg, EPG_EVENT_STATE state);

  /*
   * Epg Handling
   */
  void        CreateEvent     ( const tvheadend::entity::Event &event, EPG_TAG &epg );
  void        TransferEvent   ( const tvheadend::entity::Event &event, EPG_EVENT_STATE state );
  void        TransferEvent   ( ADDON_HANDLE handle, const tvheadend::entity::Event &event );

  /*
   * Message sending
   */
  PVR_ERROR   SendDvrDelete   ( uint32_t id, const char *method );
  PVR_ERROR   SendDvrUpdate   ( htsmsg_t *m );

  /*
   * Channel/Tags/Recordings/Events
   */
  void SyncChannelsCompleted     ( void );
  void SyncDvrCompleted          ( void );
  void SyncEpgCompleted          ( void );
  void SyncCompleted             ( void );
  void ParseTagAddOrUpdate       ( htsmsg_t *m, bool bAdd );
  void ParseTagDelete            ( htsmsg_t *m );
  void ParseChannelAddOrUpdate   ( htsmsg_t *m, bool bAdd );
  void ParseChannelDelete        ( htsmsg_t *m );
  void ParseRecordingAddOrUpdate ( htsmsg_t *m, bool bAdd );
  void ParseRecordingDelete      ( htsmsg_t *m );
  void ParseEventAddOrUpdate     ( htsmsg_t *m, bool bAdd );
  void ParseEventDelete          ( htsmsg_t *m );
  bool ParseEvent                ( htsmsg_t *msg, bool bAdd, tvheadend::entity::Event &evt );

  /*
   * VFS
   */
  bool VfsIsActiveRecording() const { return m_playingRecording && m_playingRecording->GetState() == PVR_TIMER_STATE_RECORDING; }

public:
  /*
   * Connection (pass-thru)
   */
  std::string GetServerName() const;
  std::string GetServerVersion() const;
  std::string GetServerString() const;
  int GetProtocol() const;
  bool HasCapability(const std::string &capability) const;
  void OnSleep();
  void OnWake();

  /*
   * Demuxer
   */
  bool         DemuxOpen           ( const PVR_CHANNEL &chn );
  void         DemuxClose          ( void );
  DemuxPacket *DemuxRead           ( void );
  void         DemuxFlush          ( void );
  void         DemuxAbort          ( void );
  bool         DemuxSeek           ( double time, bool backward, double *startpts );
  void         DemuxSpeed          ( int speed );
  PVR_ERROR    DemuxCurrentStreams ( PVR_STREAM_PROPERTIES *streams );
  PVR_ERROR    DemuxCurrentSignal  ( PVR_SIGNAL_STATUS &sig );
  PVR_ERROR    DemuxCurrentDescramble( PVR_DESCRAMBLE_INFO *info);
  bool         DemuxIsTimeShifting() const;
  bool         DemuxIsRealTimeStream() const;

  void CloseExpiredSubscriptions();

  /*
   * VFS (pass-thru)
   */
  bool VfsOpen(const PVR_RECORDING &rec);
  void VfsClose();
  ssize_t VfsRead(unsigned char *buf, unsigned int len);
  long long VfsSeek(long long position, int whence);
  long long VfsSize();
  void VfsPauseStream(bool paused);
  bool VfsIsRealTimeStream();

  /*
   * stream times (live streams and recordings)
   */
  PVR_ERROR GetStreamTimes(PVR_STREAM_TIMES *times);

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
