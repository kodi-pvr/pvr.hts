/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "entity/Event.h"
#include "kodi/libXBMC_pvr.h"

#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace tvheadend
{

typedef enum
{
  DVR_PRIO_IMPORTANT = 0,
  DVR_PRIO_HIGH = 1,
  DVR_PRIO_NORMAL = 2,
  DVR_PRIO_LOW = 3,
  DVR_PRIO_UNIMPORTANT = 4,
  DVR_PRIO_NOT_SET = 5,
  DVR_PRIO_DEFAULT = 6
} dvr_prio_t;

typedef enum
{
  DVR_ACTION_TYPE_CUT,
  DVR_ACTION_TYPE_MUTE,
  DVR_ACTION_TYPE_SCENE,
  DVR_ACTION_TYPE_COMBREAK,

} dvr_action_type_t;

typedef enum
{
  DVR_AUTOREC_RECORD_ALL = 0,
  DVR_AUTOREC_RECORD_UNIQUE = 14, // Unique episode in EPG/XMLTV
  DVR_AUTOREC_RECORD_DIFFERENT_EPISODE_NUMBER = 1,
  DVR_AUTOREC_RECORD_DIFFERENT_SUBTITLE = 2,
  DVR_AUTOREC_RECORD_DIFFERENT_DESCRIPTION = 3,
  DVR_AUTOREC_RECORD_ONCE_PER_MONTH = 12,
  DVR_AUTOREC_RECORD_ONCE_PER_WEEK = 4,
  DVR_AUTOREC_RECORD_ONCE_PER_DAY = 5,
  DVR_AUTOREC_LRECORD_DIFFERENT_EPISODE_NUMBER = 6,
  DVR_AUTOREC_LRECORD_DIFFERENT_TITLE = 7,
  DVR_AUTOREC_LRECORD_DIFFERENT_SUBTITLE = 8,
  DVR_AUTOREC_LRECORD_DIFFERENT_DESCRIPTION = 9,
  DVR_AUTOREC_LRECORD_ONCE_PER_MONTH = 13,
  DVR_AUTOREC_LRECORD_ONCE_PER_WEEK = 10,
  DVR_AUTOREC_LRECORD_ONCE_PER_DAY = 11,
} dvr_autorec_dedup_t;

/*
 * Defines for both the retention and removal of a dvr entry
 * retention = lifetime of the database entry
 * removal   = lifetime of the actual recording on disk
 */
typedef enum
{
  DVR_RET_DVRCONFIG = 0, // the server will use it's own default value
  DVR_RET_1DAY = 1, // the server will delete the db entry or recording after 1 day
  DVR_RET_3DAY = 3, // ...
  DVR_RET_5DAY = 5,
  DVR_RET_1WEEK = 7,
  DVR_RET_2WEEK = 14,
  DVR_RET_3WEEK = 21,
  DVR_RET_1MONTH = 31,
  DVR_RET_2MONTH = 62,
  DVR_RET_3MONTH = 92,
  DVR_RET_6MONTH = 183,
  DVR_RET_1YEAR = 366,
  DVR_RET_2YEARS = 731,
  DVR_RET_3YEARS = 1096,
  DVR_RET_SPACE =
      INT32_MAX -
      1, // the server may delete this recording if space for a new recording is needed (removal only)
  DVR_RET_FOREVER =
      INT32_MAX // the server should never delete this recording or database entry, only the user can do this
} dvr_retention_t;

typedef enum
{
  CHANNEL_TYPE_OTHER = 0,
  CHANNEL_TYPE_TV = 1,
  CHANNEL_TYPE_RADIO = 2
} channel_type_t;

typedef enum
{
  HTSP_DVR_PLAYCOUNT_RESET = 0,
  HTSP_DVR_PLAYCOUNT_SET = 1,
  HTSP_DVR_PLAYCOUNT_KEEP = INT32_MAX - 1,
  HTSP_DVR_PLAYCOUNT_INCR = INT32_MAX
} dvr_playcount_t;

enum eHTSPEventType
{
  HTSP_EVENT_NONE = 0,
  HTSP_EVENT_CHN_UPDATE = 1,
  HTSP_EVENT_TAG_UPDATE = 2,
  HTSP_EVENT_EPG_UPDATE = 3,
  HTSP_EVENT_REC_UPDATE = 4,
};

struct SHTSPEvent
{
  eHTSPEventType m_type;

  // params for HTSP_EVENT_EPG_UPDATE
  tvheadend::entity::Event m_epg;
  EPG_EVENT_STATE m_state;

  SHTSPEvent(eHTSPEventType type = HTSP_EVENT_NONE) : m_type(type), m_state(EPG_EVENT_CREATED) {}

  SHTSPEvent(eHTSPEventType type, const tvheadend::entity::Event& epg, EPG_EVENT_STATE state)
    : m_type(type), m_epg(epg), m_state(state)
  {
  }

  bool operator==(const SHTSPEvent& right) const
  {
    return m_type == right.m_type && m_epg == right.m_epg && m_state && right.m_state;
  }

  bool operator!=(const SHTSPEvent& right) const { return !(*this == right); }
};

typedef std::vector<SHTSPEvent> SHTSPEventList;

} // namespace tvheadend
