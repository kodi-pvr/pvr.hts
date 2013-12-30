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

#include <deque>
#include <algorithm>
#include <vector>
#include <map>
#include "platform/util/StdString.h"
#include "libXBMC_codec.h"
#include "client.h"
#include "xbmc_codec_descriptor.hpp"

typedef std::vector<CodecDescriptor> CodecVector;

struct STag
{
  int              id;
  std::string      name;
  std::string      icon;
  std::vector<int> channels;

  STag() { Clear(); }
  void Clear()
  {
    id    = 0;
    name.clear();
    icon.clear();
    channels.clear();
  }
  bool BelongsTo(int channel) const
  {
    return std::find(channels.begin(), channels.end(), channel) != channels.end();
  }

};

struct SChannel
{
  int              id;
  std::string      name;
  std::string      icon;
  int              event;
  int              num;
  bool             radio;
  int              caid;
  std::vector<int> tags;

  SChannel() { Clear(); }
  void Clear()
  {
    id    = 0;
    event = 0;
    num   = 0;
    radio = false;
    caid  = 0;
    name.clear();
    icon.clear();
    tags.clear();
  }
  bool MemberOf(int tag) const
  {
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
  }
  bool operator<(const SChannel &right) const
  {
    return num < right.num;
  }
};

struct SEvent
{
  int         id;
  int         next;
  int         chan_id;

  int         content;
  int         start;
  int         stop;
  std::string title;
  std::string descs;

  SEvent() { Clear(); }
  void Clear()
  {
    id    = 0;
    next  = 0;
    start = 0;
    stop  = 0;
    title.clear();
    descs.clear();
  }
};

typedef enum recording_state {
  ST_INVALID   = 0,
  ST_SCHEDULED = 1,
  ST_RECORDING = 2,
  ST_COMPLETED = 3,
  ST_ABORTED   = 4
} ERecordingState;

struct SRecording
{
  uint32_t         id;
  uint32_t         channel;
  uint32_t         start;
  uint32_t         stop;
  std::string      title;
  std::string      path;
  std::string      description;
  ERecordingState  state;
  std::string      error;

  SRecording() { Clear(); }
  void Clear()
  {
    id = channel = start = stop = 0;
    title.clear();
    description.clear();
    state = ST_INVALID;
    error.clear();
  }
};

struct SQueueStatus
{
  uint32_t packets; // Number of data packets in queue.
  uint32_t bytes;   // Number of bytes in queue.
  uint32_t delay;   // Estimated delay of queue (in µs)
  uint32_t bdrops;  // Number of B-frames dropped
  uint32_t pdrops;  // Number of P-frames dropped
  uint32_t idrops;  // Number of I-frames dropped

  SQueueStatus() { Clear(); }
  void Clear()
  {
    packets = 0;
    bytes   = 0;
    delay   = 0;
    bdrops  = 0;
    pdrops  = 0;
    idrops  = 0;
  }
};

struct SQuality
{
  std::string fe_status;
  uint32_t    fe_snr;
  uint32_t    fe_signal;
  uint32_t    fe_ber;
  uint32_t    fe_unc;
};

struct SSourceInfo
{
  std::string si_adapter;
  std::string si_network;
  std::string si_mux;
  std::string si_provider;
  std::string si_service;
};

typedef std::map<int, SChannel>   SChannels;
typedef std::map<int, STag>       STags;
typedef std::map<int, SEvent>     SEvents;
typedef std::map<int, SRecording> SRecordings;
