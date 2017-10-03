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

#include <algorithm>
#include <atomic>
#include <map>
#include <utility>
#include <vector>

#include "client.h"
#include "p8-platform/sockets/tcp.h"
#include "p8-platform/threads/threads.h"
#include "p8-platform/threads/mutex.h"
#include "p8-platform/util/buffer.h"
#include "libXBMC_addon.h"
#include "tvheadend/Settings.h"
#include "HTSPTypes.h"
#include "tvheadend/ChannelTuningPredictor.h"
#include "tvheadend/Profile.h"
#include "tvheadend/entity/Tag.h"
#include "tvheadend/entity/Channel.h"
#include "tvheadend/entity/Recording.h"
#include "tvheadend/entity/Event.h"
#include "tvheadend/entity/Schedule.h"
#include "tvheadend/status/DescrambleInfo.h"
#include "tvheadend/status/Quality.h"
#include "tvheadend/status/SourceInfo.h"
#include "tvheadend/status/TimeshiftStatus.h"
#include "tvheadend/utilities/AsyncState.h"
#include "tvheadend/Subscription.h"
#include "TimeRecordings.h"
#include "AutoRecordings.h"


#include "HTSPMessage.h"
#include "IHTSPConnectionListener.h"

#define UNNUMBERED_CHANNEL      (10000)
#define INVALID_SEEKTIME           (-1)
#define SPEED_NORMAL             (1000) // x1 playback speed

/*
 * Forward decleration of classes
 */
class CHTSPConnection;
class CHTSPDemuxer;
class CHTSPVFS;

/* Typedefs */
typedef P8PLATFORM::SyncedBuffer<CHTSPMessage> CHTSPMessageQueue;

/*
 * HTSP Demuxer - live streams
 */
class CHTSPDemuxer
{
  friend class CTvheadend;

public:
  CHTSPDemuxer( CHTSPConnection &conn );
  ~CHTSPDemuxer();

  bool   ProcessMessage ( const char *method, htsmsg_t *m );
  void   Connected      ( void );

  bool IsTimeShifting() const;
  bool IsRealTimeStream() const;
  int64_t GetTimeshiftTime() const;
  int64_t GetTimeshiftBufferStart() const;
  int64_t GetTimeshiftBufferEnd() const;
  uint32_t GetSubscriptionId() const;
  uint32_t GetChannelId() const;
  time_t GetLastUse() const;

  /**
   * Tells each demuxer to use the specified profile for new subscriptions
   * @param profile the profile to use
   */
  void SetStreamingProfile(const std::string &profile);

private:
  mutable P8PLATFORM::CMutex              m_mutex;
  CHTSPConnection                        &m_conn;
  P8PLATFORM::SyncedBuffer<DemuxPacket*>  m_pktBuffer;
  std::vector<PVR_STREAM_PROPERTIES::PVR_STREAM> m_streams;
  std::map<int,int>                       m_streamStat;
  int64_t                                 m_seekTime;
  P8PLATFORM::CCondition<volatile int64_t>  m_seekCond;
  bool                                    m_seeking;
  bool                                    m_speedChange;
  tvheadend::status::SourceInfo           m_sourceInfo;
  tvheadend::status::Quality              m_signalInfo;
  tvheadend::status::TimeshiftStatus      m_timeshiftStatus;
  tvheadend::status::DescrambleInfo       m_descrambleInfo;
  tvheadend::Subscription                 m_subscription;
  std::atomic<time_t>                     m_lastUse;
  
  void         Close0         ( void );
  void         Abort0         ( void );
  bool         Open           ( uint32_t channelId,
                                tvheadend::eSubscriptionWeight weight = tvheadend::SUBSCRIPTION_WEIGHT_NORMAL );
  void         Close          ( void );
  DemuxPacket *Read           ( void );
  void         Trim           ( void );
  void         Flush          ( void );
  void         Abort          ( void );
  bool         Seek           ( double time, bool backwards, double *startpts );
  void         Speed          ( int speed );
  void         Weight         ( tvheadend::eSubscriptionWeight weight );
  PVR_ERROR    CurrentStreams ( PVR_STREAM_PROPERTIES *streams );
  PVR_ERROR    CurrentSignal  ( PVR_SIGNAL_STATUS &sig );
  PVR_ERROR    CurrentDescrambleInfo ( PVR_DESCRAMBLE_INFO *info );

  /**
   * Resets the signal and quality info
   */
  void ResetStatus();
  
  void ParseMuxPacket           ( htsmsg_t *m );
  void ParseSourceInfo          ( htsmsg_t *m );
  void ParseSubscriptionStart   ( htsmsg_t *m );
  void ParseSubscriptionStop    ( htsmsg_t *m );
  void ParseSubscriptionSkip    ( htsmsg_t *m );
  void ParseSubscriptionSpeed   ( htsmsg_t *m );
  void ParseQueueStatus         ( htsmsg_t *m );
  void ParseSignalStatus        ( htsmsg_t *m );
  void ParseTimeshiftStatus     ( htsmsg_t *m );
  void ParseDescrambleInfo      ( htsmsg_t *m );
};

/*
 * HTSP VFS - recordings
 */
class CHTSPVFS 
{
  friend class CTvheadend;

public:
  CHTSPVFS ( CHTSPConnection &conn );
  ~CHTSPVFS ();

  void Connected    ( void );

private:
  CHTSPConnection &m_conn;
  std::string     m_path;
  uint32_t        m_fileId;
  int64_t         m_offset;

  bool      Open   ( const PVR_RECORDING &rec );
  void      Close  ( void );
  ssize_t   Read   ( unsigned char *buf, unsigned int len );
  long long Seek   ( long long pos, int whence );
  long long Tell   ( void );
  long long Size   ( void );

  bool      SendFileOpen  ( bool force = false );
  void      SendFileClose ( void );
  ssize_t   SendFileRead  ( unsigned char *buf, unsigned int len );
  long long SendFileSeek  ( int64_t pos, int whence, bool force = false );
};

/*
 * Root object for Tvheadend connection
 */
class CTvheadend
  : public P8PLATFORM::CThread, public IHTSPConnectionListener
{
public:
  CTvheadend(PVR_PROPERTIES *pvrProps);
  ~CTvheadend() override;

  void Start ( void );

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

  /**
   * The streaming profiles available on the server
   */
  tvheadend::Profiles         m_profiles;

  P8PLATFORM::CMutex          m_mutex;

  CHTSPConnection*            m_conn;

  std::vector<CHTSPDemuxer*>  m_dmx;
  CHTSPDemuxer*               m_dmx_active;
  bool                        m_streamchange;
  CHTSPVFS                    m_vfs;

  CHTSPMessageQueue           m_queue;

  tvheadend::entity::Channels   m_channels;
  tvheadend::entity::Tags       m_tags;
  tvheadend::entity::Recordings m_recordings;
  tvheadend::entity::Schedules  m_schedules;

  tvheadend::ChannelTuningPredictor m_channelTuningPredictor;

  SHTSPEventList              m_events;

  tvheadend::utilities::AsyncState  m_asyncState;

  TimeRecordings              m_timeRecordings;
  AutoRecordings              m_autoRecordings;

  int                         m_epgMaxDays;

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
  inline void TriggerChannelGroupsUpdate ( void )
  {
    m_events.emplace_back(SHTSPEvent(HTSP_EVENT_TAG_UPDATE));
  }
  inline void TriggerChannelUpdate ( void )
  {
    m_events.emplace_back(SHTSPEvent(HTSP_EVENT_CHN_UPDATE));
  }
  inline void TriggerRecordingUpdate ( void )
  {
    m_events.emplace_back(SHTSPEvent(HTSP_EVENT_REC_UPDATE));
  }
  inline void TriggerTimerUpdate ( void )
  {
    m_events.emplace_back(SHTSPEvent(HTSP_EVENT_REC_UPDATE));
  }
  inline void PushEpgEventUpdate ( const tvheadend::entity::Event &epg, EPG_EVENT_STATE state )
  {
    SHTSPEvent event = SHTSPEvent(HTSP_EVENT_EPG_UPDATE, epg, state);

    if (std::find(m_events.begin(), m_events.end(), event) == m_events.end())
      m_events.emplace_back(event);
  }

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
  int64_t      DemuxGetTimeshiftTime() const;
  int64_t      DemuxGetTimeshiftBufferStart() const;
  int64_t      DemuxGetTimeshiftBufferEnd() const;
  bool         DemuxIsTimeShifting() const;
  bool         DemuxIsRealTimeStream() const;

  /*
   * VFS (pass-thru)
   */
  inline bool         VfsOpen             ( const PVR_RECORDING &rec )
  {
    return m_vfs.Open(rec);
  }
  inline void         VfsClose            ( void )
  {
    m_vfs.Close();
  }
  inline ssize_t       VfsRead             ( unsigned char *buf, unsigned int len )
  {
    return m_vfs.Read(buf, len);
  }
  inline long long    VfsSeek             ( long long position, int whence )
  {
    return m_vfs.Seek(position, whence);
  }
  inline long long    VfsTell             ( void )
  {
    return m_vfs.Tell();
  }
  inline long long    VfsSize             ( void )
  {
    return m_vfs.Size();
  }
};
