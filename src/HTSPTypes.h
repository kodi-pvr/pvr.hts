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
