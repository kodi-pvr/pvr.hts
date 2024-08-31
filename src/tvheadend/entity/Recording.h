/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "RecordingBase.h"

#include "kodi/addon-instance/pvr/Timers.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>

// Timer types
#define TIMER_ONCE_MANUAL (PVR_TIMER_TYPE_NONE + 1)
#define TIMER_ONCE_EPG (PVR_TIMER_TYPE_NONE + 2)
#define TIMER_ONCE_CREATED_BY_TIMEREC (PVR_TIMER_TYPE_NONE + 3)
#define TIMER_ONCE_CREATED_BY_AUTOREC (PVR_TIMER_TYPE_NONE + 4)
#define TIMER_REPEATING_MANUAL (PVR_TIMER_TYPE_NONE + 5)
#define TIMER_REPEATING_EPG (PVR_TIMER_TYPE_NONE + 6)
#define TIMER_REPEATING_SERIESLINK (PVR_TIMER_TYPE_NONE + 7)

namespace tvheadend::entity
{

class Recording;
typedef std::pair<uint32_t, Recording> RecordingMapEntry;
typedef std::map<uint32_t, Recording> Recordings;

/**
 * Represents a recording or a timer
 * TODO: Create separate classes for recordings and timers since a
 * recording obviously can't have a "timer type"
 */
class Recording : public RecordingBase
{
public:
  Recording() = default;

  bool operator==(const Recording& other)
  {
    return RecordingBase::operator==(other) && m_channelType == other.m_channelType &&
           m_channelName == other.m_channelName && m_eventId == other.m_eventId &&
           m_start == other.m_start && m_stop == other.m_stop &&
           m_startExtra == other.m_startExtra && m_stopExtra == other.m_stopExtra &&
           m_filesStart == other.m_filesStart && m_filesStop == other.m_filesStop &&
           m_filesSize == other.m_filesSize && m_subtitle == other.m_subtitle &&
           m_path == other.m_path && m_description == other.m_description &&
           m_image == other.m_image && m_fanartImage == other.m_fanartImage &&
           m_timerecId == other.m_timerecId && m_autorecId == other.m_autorecId &&
           m_state == other.m_state && m_error == other.m_error &&
           m_playCount == other.m_playCount && m_playPosition == other.m_playPosition &&
           m_contentType == other.m_contentType && m_season == other.m_season &&
           m_episode == other.m_episode && m_part == other.m_part &&
           m_ageRating == other.m_ageRating && m_ratingLabel == other.m_ratingLabel &&
           m_ratingIcon == other.m_ratingIcon && m_ratingSource == other.m_ratingSource;
  }

  bool operator!=(const Recording& other) { return !(*this == other); }

  bool IsRecording() const
  {
    return m_state == PVR_TIMER_STATE_COMPLETED || m_state == PVR_TIMER_STATE_ABORTED ||
           m_state == PVR_TIMER_STATE_RECORDING || m_state == PVR_TIMER_STATE_CONFLICT_NOK;
  }

  bool IsTimer() const
  {
    return m_state == PVR_TIMER_STATE_SCHEDULED || m_state == PVR_TIMER_STATE_RECORDING ||
           m_state == PVR_TIMER_STATE_CONFLICT_NOK;
  }

  /**
   * @return the type of timer
   */
  unsigned int GetTimerType() const
  {
    if (!m_timerecId.empty())
      return TIMER_ONCE_CREATED_BY_TIMEREC;
    else if (!m_autorecId.empty())
      return TIMER_ONCE_CREATED_BY_AUTOREC;
    else if (m_eventId != 0)
      return TIMER_ONCE_EPG;
    else
      return TIMER_ONCE_MANUAL;
  }

  uint32_t GetChannelType() const { return m_channelType; }
  void SetChannelType(uint32_t channelType) { m_channelType = channelType; }

  const std::string& GetChannelName() const { return m_channelName; }
  void SetChannelName(const std::string& channelName) { m_channelName = channelName; }

  uint32_t GetEventId() const { return m_eventId; }
  void SetEventId(uint32_t eventId) { m_eventId = eventId; }

  //! @todo Change to time_t
  int64_t GetStart() const { return m_start; }
  void SetStart(int64_t start) { m_start = start; }

  //! @todo Change to time_t
  int64_t GetStop() const { return m_stop; }
  void SetStop(int64_t stop) { m_stop = stop; }

  //! @todo Change to time_t
  int64_t GetStartExtra() const { return m_startExtra; }
  void SetStartExtra(int64_t startExtra) { m_startExtra = startExtra; }

  //! @todo Change to time_t
  int64_t GetStopExtra() const { return m_stopExtra; }
  void SetStopExtra(int64_t stopExtra) { m_stopExtra = stopExtra; }

  int64_t GetFilesStart() const { return m_filesStart; }
  void SetFilesStart(int64_t start) { m_filesStart = start; }

  int64_t GetFilesStop() const { return m_filesStop; }
  void SetFilesStop(int64_t stop) { m_filesStop = stop; }

  int64_t GetFilesSize() const { return m_filesSize; }
  void SetFilesSize(int64_t size) { m_filesSize = size; }

  const std::string& GetSubtitle() const { return m_subtitle; }
  void SetSubtitle(const std::string& subtitle) { m_subtitle = subtitle; }

  const std::string& GetPath() const { return m_path; }
  void SetPath(const std::string& path) { m_path = path; }

  const std::string& GetDescription() const { return m_description; }
  void SetDescription(const std::string& description) { m_description = description; }

  const std::string& GetImage() const { return m_image; }
  void SetImage(const std::string& image) { m_image = image; }

  const std::string& GetFanartImage() const { return m_fanartImage; }
  void SetFanartImage(const std::string& image) { m_fanartImage = image; }

  const std::string& GetTimerecId() const { return m_timerecId; }
  void SetTimerecId(const std::string& autorecId) { m_timerecId = autorecId; }

  const std::string& GetAutorecId() const { return m_autorecId; }
  void SetAutorecId(const std::string& title) { m_autorecId = title; }

  PVR_TIMER_STATE GetState() const { return m_state; }
  void SetState(const PVR_TIMER_STATE& state) { m_state = state; }

  const std::string& GetError() const { return m_error; }
  void SetError(const std::string& error) { m_error = error; }

  uint32_t GetPlayCount() const { return m_playCount; }
  void SetPlayCount(uint32_t playCount) { m_playCount = playCount; }

  uint32_t GetPlayPosition() const { return m_playPosition; }
  void SetPlayPosition(uint32_t playPosition) { m_playPosition = playPosition; }

  void SetContentType(uint32_t content) { m_contentType = content; }
  uint32_t GetContentType() const { return m_contentType; }
  // tvh returns only the major DVB category for recordings in the
  // bottom four bits and no sub-category
  uint32_t GetGenreType() const { return m_contentType * 0x10; }
  uint32_t GetGenreSubType() const { return 0; }

  int32_t GetSeason() const { return m_season; }
  void SetSeason(int32_t season) { m_season = season; }

  int32_t GetEpisode() const { return m_episode; }
  void SetEpisode(int32_t episode) { m_episode = episode; }

  uint32_t GetPart() const { return m_part; }
  void SetPart(uint32_t part) { m_part = part; }

  void SetAgeRating(uint32_t content) { m_ageRating = content; }
  uint32_t GetAgeRating() const { return m_ageRating; }

  const std::string& GetRatingLabel() const { return m_ratingLabel; }
  void SetRatingLabel(const std::string& ratingLabel) { m_ratingLabel = ratingLabel; }

  const std::string& GetRatingIcon() const { return m_ratingIcon; }
  void SetRatingIcon(const std::string& ratingIcon) { m_ratingIcon = ratingIcon; }

  const std::string& GetRatingSource() const { return m_ratingSource; }
  void SetRatingSource(const std::string& ratingSource) { m_ratingSource = ratingSource; }

private:
  uint32_t m_channelType{0};
  std::string m_channelName;
  uint32_t m_eventId{0};
  int64_t m_start{0};
  int64_t m_stop{0};
  int64_t m_startExtra{0};
  int64_t m_stopExtra{0};
  int64_t m_filesStart{0};
  int64_t m_filesStop{0};
  int64_t m_filesSize{0};
  std::string m_subtitle;
  std::string m_path;
  std::string m_description;
  std::string m_image;
  std::string m_fanartImage;
  std::string m_timerecId;
  std::string m_autorecId;
  PVR_TIMER_STATE m_state{PVR_TIMER_STATE_ERROR};
  std::string m_error;
  uint32_t m_playCount{0};
  uint32_t m_playPosition{0};
  uint32_t m_contentType{0};
  int32_t m_season{-1};
  int32_t m_episode{-1};
  uint32_t m_part{0};
  uint32_t m_ageRating{0};
  std::string m_ratingLabel;
  std::string m_ratingIcon;
  std::string m_ratingSource;
};

} // namespace tvheadend::entity
