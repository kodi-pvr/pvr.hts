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

#include "client.h"
#include "p8-platform/sockets/tcp.h"
#include "p8-platform/threads/threads.h"
#include "p8-platform/threads/mutex.h"
#include "p8-platform/util/buffer.h"
#include "kodi/xbmc_codec_types.h"
#include "kodi/libXBMC_addon.h"
#include "tvheadend/Settings.h"
#include "HTSPTypes.h"
#include "tvheadend/ChannelTuningPredictor.h"
#include "tvheadend/Profile.h"
#include "tvheadend/entity/Tag.h"
#include "tvheadend/entity/Channel.h"
#include "tvheadend/entity/Recording.h"
#include "tvheadend/entity/Event.h"
#include "tvheadend/entity/Schedule.h"
#include "tvheadend/status/Quality.h"
#include "tvheadend/status/SourceInfo.h"
#include "tvheadend/status/TimeshiftStatus.h"
#include "tvheadend/utilities/AsyncState.h"
#include "tvheadend/Subscription.h"
#include "TimeRecordings.h"
#include "AutoRecordings.h"
#include <map>
#include <queue>
#include <cstdarg>
#include <stdexcept>
#include <atomic>

extern "C" {
#include <sys/types.h>
#include "libhts/htsmsg.h"
}

/*
 * Miscellaneous
 */
#if defined(__GNUC__)
#define _unused(x) x __attribute__((unused))
#else
#define _unused(x) x
#endif

/*
 * Configuration defines
 */
#define HTSP_MIN_SERVER_VERSION       (19) // Server must support at least this htsp version
#define HTSP_CLIENT_VERSION           (25) // Client uses HTSP features up to this version. If the respective
                                           // addon feature requires htsp features introduced after
                                           // HTSP_MIN_SERVER_VERSION this feature will only be available if the
                                           // actual server HTSP version matches (runtime htsp version check).
#define FAST_RECONNECT_ATTEMPTS     (5)
#define FAST_RECONNECT_INTERVAL   (500) // ms
#define UNNUMBERED_CHANNEL      (10000)
#define INVALID_SEEKTIME           (-1)

/*
 * Forward decleration of classes
 */
class CHTSPConnection;
class CHTSPDemuxer;
class CHTSPVFS;
class CHTSPResponse;
class CHTSPMessage;

/* Typedefs */
typedef std::map<uint32_t,CHTSPResponse*> CHTSPResponseList;
typedef P8PLATFORM::SyncedBuffer<CHTSPMessage> CHTSPMessageQueue;

/*
 * HTSP Response handler
 */
class CHTSPResponse
{
public:
  CHTSPResponse();
  ~CHTSPResponse();
  htsmsg_t *Get ( P8PLATFORM::CMutex &mutex, uint32_t timeout );
  void      Set ( htsmsg_t *m );
private:
  P8PLATFORM::CCondition<volatile bool> m_cond;
  bool                                m_flag;
  htsmsg_t                           *m_msg;
};

/*
 * HTSP Message
 */
class CHTSPMessage
{
public:
  CHTSPMessage(std::string method = "", htsmsg_t *msg = NULL)
    : m_method(method), m_msg(msg)
  {
  }
  CHTSPMessage(const CHTSPMessage& msg)
    : m_method(msg.m_method), m_msg(msg.m_msg)
  {
    msg.m_msg    = NULL;
  }
  ~CHTSPMessage()
  {
    if (m_msg)
      htsmsg_destroy(m_msg);
  }
  CHTSPMessage& operator=(const CHTSPMessage &msg)
  {
    if (this != &msg)
    {
      if (m_msg)
        htsmsg_destroy(m_msg);
      m_method     = msg.m_method;
      m_msg        = msg.m_msg;
      msg.m_msg    = NULL; // ownership is passed
    }
    return *this;
  }
  std::string      m_method;
  mutable htsmsg_t *m_msg;
};

/*
 * HTSP Connection registration thread
 */
class CHTSPRegister
  : public P8PLATFORM::CThread
{
  friend class CHTSPConnection;

public:
  CHTSPRegister ( CHTSPConnection *conn );
  ~CHTSPRegister ();
 
private:
  CHTSPConnection *m_conn;
  void *Process ( void );
};

/*
 * HTSP Connection
 */
class CHTSPConnection
  : public P8PLATFORM::CThread
{
  friend class CHTSPRegister;

public:
  CHTSPConnection();
  ~CHTSPConnection();

  void Disconnect  ( void );
  
  bool      SendMessage0    ( const char *method, htsmsg_t *m );
  htsmsg_t *SendAndWait0    ( const char *method, htsmsg_t *m, int iResponseTimeout = -1);
  htsmsg_t *SendAndWait     ( const char *method, htsmsg_t *m, int iResponseTimeout = -1 );

  int  GetProtocol             ( void ) const;

  std::string GetWebURL        ( const char *fmt, ... ) const;

  std::string GetServerName    ( void ) const;
  std::string GetServerVersion ( void ) const;
  std::string GetServerString  ( void ) const;
  
  bool        HasCapability(const std::string &capability) const;

  inline P8PLATFORM::CMutex& Mutex ( void ) { return m_mutex; }

  void        OnSleep ( void );
  void        OnWake  ( void );
  
private:
  void*       Process          ( void );
  void        Register         ( void );
  bool        ReadMessage      ( void );
  bool        SendHello        ( void );
  bool        SendAuth         ( const std::string &u, const std::string &p );

  void        SetState         ( PVR_CONNECTION_STATE state );
  bool        WaitForConnection( void );

  P8PLATFORM::CTcpSocket              *m_socket;
  mutable P8PLATFORM::CMutex          m_mutex;
  CHTSPRegister                       m_regThread;
  P8PLATFORM::CCondition<volatile bool> m_regCond;
  bool                                m_ready;
  uint32_t                            m_seq;
  std::string                         m_serverName;
  std::string                         m_serverVersion;
  int                                 m_htspVersion;
  std::string                         m_webRoot;
  void*                               m_challenge;
  int                                 m_challengeLen;

  CHTSPResponseList                   m_messages;
  std::vector<std::string>            m_capabilities;

  bool                                m_suspended;
  PVR_CONNECTION_STATE                m_state;
};

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
  PVR_STREAM_PROPERTIES                   m_streams;
  std::map<int,int>                       m_streamStat;
  int64_t                                 m_seekTime;
  P8PLATFORM::CCondition<volatile int64_t>  m_seekCond;
  bool                                    m_seeking;
  bool                                    m_speedChange;
  tvheadend::status::SourceInfo           m_sourceInfo;
  tvheadend::status::Quality              m_signalInfo;
  tvheadend::status::TimeshiftStatus      m_timeshiftStatus;
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
  bool         Seek           ( int time, bool backwards, double *startpts );
  void         Speed          ( int speed );
  void         Weight         ( tvheadend::eSubscriptionWeight weight );
  PVR_ERROR    CurrentStreams ( PVR_STREAM_PROPERTIES *streams );
  PVR_ERROR    CurrentSignal  ( PVR_SIGNAL_STATUS &sig );

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
  : public P8PLATFORM::CThread
{
public:
  CTvheadend(PVR_PROPERTIES *pvrProps);
  ~CTvheadend();

  void Start ( void );

  void Disconnected   ( void );
  bool Connected      ( void );
  bool ProcessMessage ( const char *method, htsmsg_t *msg );

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
  PVR_ERROR GetTimerTypes     ( PVR_TIMER_TYPE types[], int *size );
  int       GetTimerCount     ( void );
  PVR_ERROR GetTimers         ( ADDON_HANDLE handle );
  PVR_ERROR AddTimer          ( const PVR_TIMER &tmr );
  PVR_ERROR DeleteTimer       ( const PVR_TIMER &tmr, bool force );
  PVR_ERROR UpdateTimer       ( const PVR_TIMER &tmr );

  PVR_ERROR GetEpg            ( ADDON_HANDLE handle, const PVR_CHANNEL &chn,
                                time_t start, time_t end );
  PVR_ERROR SetEPGTimeFrame   ( int iDays );

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

  P8PLATFORM::CMutex            m_mutex;

  CHTSPConnection             m_conn;

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
   * Message processing
   */
  void *Process ( void );

  /*
   * Event handling
   */
  inline void TriggerChannelGroupsUpdate ( void )
  {
    m_events.push_back(SHTSPEvent(HTSP_EVENT_TAG_UPDATE));
  }
  inline void TriggerChannelUpdate ( void )
  {
    m_events.push_back(SHTSPEvent(HTSP_EVENT_CHN_UPDATE));
  }
  inline void TriggerRecordingUpdate ( void )
  {
    m_events.push_back(SHTSPEvent(HTSP_EVENT_REC_UPDATE));
  }
  inline void TriggerTimerUpdate ( void )
  {
    m_events.push_back(SHTSPEvent(HTSP_EVENT_REC_UPDATE));
  }
  inline void PushEpgEventUpdate ( const tvheadend::entity::Event &epg, EPG_EVENT_STATE state )
  {
    SHTSPEvent event = SHTSPEvent(HTSP_EVENT_EPG_UPDATE, epg, state);

    if (std::find(m_events.begin(), m_events.end(), event) == m_events.end())
      m_events.push_back(event);
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
  std::string GetServerName    ( void ) const
  {
    return m_conn.GetServerName();
  }
  std::string GetServerVersion ( void ) const
  {
    return m_conn.GetServerVersion();
  }
  std::string GetServerString  ( void ) const
  {
    return m_conn.GetServerString();
  }
  inline int GetProtocol ( void ) const
  {
    return m_conn.GetProtocol();
  }
  inline bool HasCapability(const std::string &capability) const
  {
    return m_conn.HasCapability(capability);
  }
  inline void OnSleep ( void )
  {
    m_conn.OnSleep();
  }
  inline void OnWake ( void )
  {
    m_conn.OnWake();
  }

  /*
   * Demuxer
   */
  bool         DemuxOpen           ( const PVR_CHANNEL &chn );
  void         DemuxClose          ( void );
  DemuxPacket *DemuxRead           ( void );
  void         DemuxFlush          ( void );
  void         DemuxAbort          ( void );
  bool         DemuxSeek           ( int time, bool backward, double *startpts );
  void         DemuxSpeed          ( int speed );
  PVR_ERROR    DemuxCurrentStreams ( PVR_STREAM_PROPERTIES *streams );
  PVR_ERROR    DemuxCurrentSignal  ( PVR_SIGNAL_STATUS &sig );
  int64_t      DemuxGetTimeshiftTime() const;
  int64_t      DemuxGetTimeshiftBufferStart() const;
  int64_t      DemuxGetTimeshiftBufferEnd() const;
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
