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
#include <string>
#include "client.h"
#include "tvheadend/entity/Event.h"

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

/*
 * Defines for both the retention and removal of a dvr entry
 * retention = lifetime of the database entry
 * removal   = lifetime of the actual recording on disk
 */
typedef enum {
  DVR_RET_DVRCONFIG = 0,            // the server will use it's own default value
  DVR_RET_1DAY      = 1,            // the server will delete the db entry or recording after 1 day
  DVR_RET_3DAY      = 3,            // ...
  DVR_RET_5DAY      = 5,
  DVR_RET_1WEEK     = 7,
  DVR_RET_2WEEK     = 14,
  DVR_RET_3WEEK     = 21,
  DVR_RET_1MONTH    = 31,
  DVR_RET_2MONTH    = 62,
  DVR_RET_3MONTH    = 92,
  DVR_RET_6MONTH    = 183,
  DVR_RET_1YEAR     = 366,
  DVR_RET_2YEARS    = 731,
  DVR_RET_3YEARS    = 1096,
  DVR_RET_ONREMOVE  = INT32_MAX-1,  // the server will delete the db entry when the actual recording gets deleted (retention only)
  DVR_RET_SPACE     = INT32_MAX-1,  // the server may delete this recording if space for a new recording is needed (removal only)
  DVR_RET_FOREVER   = INT32_MAX     // the server should never delete this recording or database entry, only the user can do this
} dvr_retention_t;

typedef enum {
  CHANNEL_TYPE_OTHER = 0,
  CHANNEL_TYPE_TV    = 1,
  CHANNEL_TYPE_RADIO = 2
} channel_type_t;

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
  EPG_EVENT_STATE          m_state;

  SHTSPEvent (eHTSPEventType type = HTSP_EVENT_NONE) :
    m_type(type),
    m_state(EPG_EVENT_CREATED)
  {
  }

  SHTSPEvent (eHTSPEventType type, const tvheadend::entity::Event &epg, EPG_EVENT_STATE state) :
    m_type(type),
    m_epg(epg),
    m_state(state)
  {
  }

  bool operator==(const SHTSPEvent &right) const
  {
    return m_type == right.m_type && m_epg == right.m_epg && m_state && right.m_state;
  }

  bool operator!=(const SHTSPEvent &right) const
  {
    return !(*this == right);
  }
};

typedef std::vector<SHTSPEvent> SHTSPEventList;
