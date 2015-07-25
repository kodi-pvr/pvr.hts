#pragma once

/*
 *      Copyright (C) 2005-2015 Team Kodi
 *      http://kodi.tv
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

#include <deque>
#include <algorithm>
#include <vector>
#include <map>
#include "client.h"

typedef enum {
  DVR_PRIO_IMPORTANT   = 0,
  DVR_PRIO_HIGH        = 1,
  DVR_PRIO_NORMAL      = 2,
  DVR_PRIO_LOW         = 3,
  DVR_PRIO_UNIMPORTANT = 4,
  DVR_PRIO_NOT_SET     = 5
} dvr_prio_t;

typedef enum {
  DVR_ACTION_TYPE_CUT,
  DVR_ACTION_TYPE_MUTE,
  DVR_ACTION_TYPE_SCENE,
  DVR_ACTION_TYPE_COMBREAK,
  
} dvr_action_type_t;

typedef enum {
  DVR_AUTOREC_RECORD_ALL = 0,
  DVR_AUTOREC_RECORD_DIFFERENT_EPISODE_NUMBER = 1,
  DVR_AUTOREC_RECORD_DIFFERENT_SUBTITLE = 2,
  DVR_AUTOREC_RECORD_DIFFERENT_DESCRIPTION = 3,
  DVR_AUTOREC_RECORD_ONCE_PER_WEEK = 4,
  DVR_AUTOREC_RECORD_ONCE_PER_DAY = 5
} dvr_autorec_dedup_t;

enum eHTSPEventType
{
  HTSP_EVENT_NONE = 0,
  HTSP_EVENT_CHN_UPDATE = 1,
  HTSP_EVENT_TAG_UPDATE = 2,
  HTSP_EVENT_EPG_UPDATE = 3,
  HTSP_EVENT_REC_UPDATE = 4,
};

enum eSubscriptionWeight {
  SUBSCRIPTION_WEIGHT_DEFAULT = 150,
  SUBSCRIPTION_WEIGHT_PRETUNING = 50,
  SUBSCRIPTION_WEIGHT_POSTTUNING = 40,
};

namespace htsp
{

class Tag
{
public:
  Tag(uint32_t id = 0);

  bool operator==(const Tag &right);
  bool operator!=(const Tag &right);

  bool IsDirty() const;
  void SetDirty(bool bDirty);

  uint32_t GetId() const;

  uint32_t GetIndex() const;
  void SetIndex(uint32_t index);

  const std::string& GetName() const;
  void SetName(const std::string& name);

  void SetIcon(const std::string& icon);

  const std::vector<uint32_t>& GetChannels() const;
  std::vector<uint32_t>& GetChannels();

  bool ContainsChannelType(bool bRadio) const;

private:
  bool                  m_dirty;

  uint32_t              m_id;
  uint32_t              m_index;
  std::string           m_name;
  std::string           m_icon;
  std::vector<uint32_t> m_channels;
};

typedef std::map<uint32_t, Tag> Tags;

class RecordingBase
{
protected:
  RecordingBase(const std::string &id = "");

  bool operator==(const RecordingBase &right);
  bool operator!=(const RecordingBase &right);

public:
  unsigned int GetIntId() const;

  bool IsDirty() const;
  void SetDirty(bool bDirty);

  std::string GetStringId() const;
  void SetStringId(const std::string &id);

  bool IsEnabled() const;
  void SetEnabled(uint32_t enabled);

  int GetDaysOfWeek() const;
  void SetDaysOfWeek(uint32_t daysOfWeek);

  uint32_t GetRetention() const;
  void SetRetention(uint32_t retention);

  uint32_t GetPriority() const;
  void SetPriority(uint32_t priority);

  const std::string& GetTitle() const;
  void SetTitle(const std::string &title);

  const std::string& GetName() const;
  void SetName(const std::string &name);

  const std::string& GetDirectory() const;
  void SetDirectory(const std::string &directory);

  void SetOwner(const std::string &owner);

  void SetCreator(const std::string &creator);

  uint32_t GetChannel() const;
  void SetChannel(uint32_t channel);

protected:
  static time_t LocaltimeToUTC(int32_t lctime);

private:
  static int    GetNextIntId();

  const unsigned int m_iId;
  bool               m_dirty;

  std::string m_id;         // ID (string!) of dvr[Time|Auto]recEntry.
  uint32_t    m_enabled;    // If [time|auto]rec entry is enabled (activated).
  uint32_t    m_daysOfWeek; // Bitmask - Days of week (0x01 = Monday, 0x40 = Sunday, 0x7f = Whole Week, 0 = Not set).
  uint32_t    m_retention;  // Retention time (in days).
  uint32_t    m_priority;   // Priority (0 = Important, 1 = High, 2 = Normal, 3 = Low, 4 = Unimportant).
  std::string m_title;      // Title (pattern) for the recording files.
  std::string m_name;       // Name.
  std::string m_directory;  // Directory for the recording files.
  std::string m_owner;      // Owner.
  std::string m_creator;    // Creator.
  uint32_t    m_channel;    // Channel ID.
};

class TimeRecording : public RecordingBase
{
public:
  TimeRecording(const std::string &id = "");

  bool operator==(const TimeRecording &right);
  bool operator!=(const TimeRecording &right);

  time_t GetStart() const;
  void SetStart(int32_t start);

  time_t GetStop() const;
  void SetStop(int32_t stop);

private:
  int32_t m_start; // Start time in minutes from midnight (up to 24*60).
  int32_t m_stop;  // Stop time in minutes from midnight (up to 24*60).
};

typedef std::map<std::string, TimeRecording> TimeRecordingsMap;

class AutoRecording : public RecordingBase
{
public:
  AutoRecording(const std::string &id = "");

  bool operator==(const AutoRecording &right);
  bool operator!=(const AutoRecording &right);

  void SetMinDuration(uint32_t minDuration);
  void SetMaxDuration(uint32_t maxDuration);

  time_t GetStart() const;
  void SetStart(int32_t start);

  time_t GetStop() const;

  int64_t GetMarginStart() const;
  void SetMarginStart(int64_t startExtra);

  int64_t GetMarginEnd() const;
  void SetMarginEnd(int64_t stopExtra);

  uint32_t GetDupDetect() const;
  void SetDupDetect(uint32_t dupDetect);

  bool GetFulltext() const;
  void SetFulltext(uint32_t fulltext);

private:
  uint32_t m_minDuration; // Minimal duration in seconds (0 = Any).
  uint32_t m_maxDuration; // Maximal duration in seconds (0 = Any).
  int32_t  m_approxTime;  // Minutes from midnight (up to 24*60).
  int64_t  m_startExtra;  // Extra start minutes (pre-time).
  int64_t  m_stopExtra;   // Extra stop minutes (post-time).
  uint32_t m_dupDetect;   // duplicate episode detect (record: 0 = all, 1 = different episode number,
                          //                                   2 = different subtitle, 3 = different description,
                          //                                   4 = once per week, 5 = once per day).
  uint32_t m_fulltext;    // Fulltext epg search.
};

typedef std::map<std::string, AutoRecording> AutoRecordingsMap;

} // namespace htsp

struct SChannel
{
  bool             del;
  uint32_t         id;
  uint32_t         num;
  uint32_t         numMinor;
  bool             radio;
  uint32_t         caid;
  std::string      name;
  std::string      icon;

  SChannel() :
    del     (false),
    id      (0),
    num     (0),
    numMinor(0),
    radio   (false),
    caid    (0)
  {
  }

  bool operator<(const SChannel &right) const
  {
    return num < right.num;
  }
};

struct SRecording
{
  bool             del;
  uint32_t         id;
  uint32_t         channel;
  uint32_t         eventId;
  int64_t          start;
  int64_t          stop;
  int64_t          startExtra;
  int64_t          stopExtra;
  std::string      title;
  std::string      path;
  std::string      description;
  std::string      timerecId;
  std::string      autorecId;
  PVR_TIMER_STATE  state;
  std::string      error;
  uint32_t         retention;
  uint32_t         priority;

  SRecording() :
    del       (false),
    id        (0),
    channel   (0),
    eventId   (0),
    start     (0),
    stop      (0),
    startExtra(0),
    stopExtra (0),
    state     (PVR_TIMER_STATE_ERROR),
    retention (99), // Kodi default - "99 days"
    priority  (DVR_PRIO_NORMAL)
  {
  }

  bool IsRecording () const
  {
    return state == PVR_TIMER_STATE_COMPLETED ||
           state == PVR_TIMER_STATE_ABORTED   ||
           state == PVR_TIMER_STATE_RECORDING;
  }

  bool IsTimer () const
  {
    return state == PVR_TIMER_STATE_SCHEDULED ||
           state == PVR_TIMER_STATE_RECORDING ||
           state == PVR_TIMER_STATE_DISABLED;
  }
};

struct SEvent
{
  bool        del;
  uint32_t    id;
  uint32_t    next;
  uint32_t    channel;
  uint32_t    content;
  time_t      start;
  time_t      stop;
  uint32_t    stars; /* 1 - 5 */
  uint32_t    age;   /* years */
  time_t      aired;
  uint32_t    season;
  uint32_t    episode;
  uint32_t    part;
  std::string title;
  std::string subtitle; /* episode name */
  std::string desc;
  std::string summary;
  std::string image;
  uint32_t    recordingId;

  SEvent() :
    del        (false),
    id         (0),
    next       (0),
    channel    (0),
    content    (0),
    start      (0),
    stop       (0),
    stars      (0),
    age        (0),
    aired      (0),
    season     (0),
    episode    (0),
    part       (0),
    recordingId(0)
  {
  }
};

typedef std::map<uint32_t, SChannel>   SChannels;
typedef std::map<uint32_t, SEvent>     SEvents;
typedef std::map<uint32_t, SRecording> SRecordings;

struct SSchedule
{
  bool     del;
  uint32_t channel;
  SEvents  events;

  SSchedule() :
    del    (false),
    channel(0)
  {
  }
};

typedef std::map<int, SSchedule>  SSchedules;

struct SQueueStatus
{
  uint32_t packets; // Number of data packets in queue.
  uint32_t bytes;   // Number of bytes in queue.
  uint32_t delay;   // Estimated delay of queue (in µs)
  uint32_t bdrops;  // Number of B-frames dropped
  uint32_t pdrops;  // Number of P-frames dropped
  uint32_t idrops;  // Number of I-frames dropped

  SQueueStatus() :
    packets(0),
    bytes  (0),
    delay  (0),
    bdrops (0),
    pdrops (0),
    idrops (0)
  {
  }
};

struct STimeshiftStatus
{
  bool    full;
  int64_t shift;
  int64_t start;
  int64_t end;
  
  STimeshiftStatus() :
    full (0),
    shift(0),
    start(0),
    end  (0)
  {
  }
};

struct SQuality
{
  std::string fe_status;
  uint32_t fe_snr;
  uint32_t fe_signal;
  uint32_t fe_ber;
  uint32_t fe_unc;
  
  SQuality() :
    fe_snr   (0),
    fe_signal(0),
    fe_ber   (0),
    fe_unc   (0)
  {
  }

  void Clear ()
  {
    fe_status.clear();
    fe_snr    = 0;
    fe_signal = 0;
    fe_ber    = 0;
    fe_unc    = 0;
  }
};

struct SSourceInfo
{
  std::string si_adapter;
  std::string si_network;
  std::string si_mux;
  std::string si_provider;
  std::string si_service;

  void Clear ()
  {
    si_adapter.clear();
    si_network.clear();
    si_mux.clear();
    si_provider.clear();
    si_service.clear();
  }
};

struct SHTSPEvent
{
  eHTSPEventType m_type;
  uint32_t       m_idx;

  SHTSPEvent (eHTSPEventType type = HTSP_EVENT_NONE, uint32_t idx = 0) :
    m_type(type),
    m_idx (idx)
  {
  }
  
  bool operator==(const SHTSPEvent &right) const
  {
    return m_type == right.m_type && m_idx == right.m_idx;
  }

  bool operator!=(const SHTSPEvent &right) const
  {
    return !(*this == right);
  }
};

typedef std::vector<SHTSPEvent> SHTSPEventList;

struct SSubscription
{
  uint32_t subscriptionId;
  uint32_t channelId;
  int      speed;
  bool     active;
  enum eSubscriptionWeight weight;

  SSubscription() :
    channelId(0),
    speed (1000),
    active(false),
    weight(SUBSCRIPTION_WEIGHT_DEFAULT)
  {
    static int previousId = 0;
    subscriptionId = ++previousId;
  }
};
