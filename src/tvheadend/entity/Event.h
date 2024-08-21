/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "Entity.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tvheadend
{
namespace entity
{

class Event;
typedef std::pair<uint32_t, Event> EventMapEntry;
typedef std::map<uint32_t, Event> Events;

/**
 * Represents an event/programme
 */
class Event : public Entity
{
public:
  Event()
    : m_next(0),
      m_channel(0),
      m_content(0),
      m_start(0),
      m_stop(0),
      m_stars(0),
      m_age(0),
      m_season(-1),
      m_episode(-1),
      m_part(-1),
      m_recordingId(0),
      m_year(0)
  {
  }

  bool operator==(const Event& other) const
  {
    return m_id == other.m_id && m_next == other.m_next && m_channel == other.m_channel &&
           m_content == other.m_content && m_start == other.m_start && m_stop == other.m_stop &&
           m_stars == other.m_stars && m_age == other.m_age && m_aired == other.m_aired &&
           m_season == other.m_season && m_episode == other.m_episode && m_part == other.m_part &&
           m_title == other.m_title && m_subtitle == other.m_subtitle && m_desc == other.m_desc &&
           m_summary == other.m_summary && m_image == other.m_image &&
           m_recordingId == other.m_recordingId && m_seriesLink == other.m_seriesLink &&
           m_year == other.m_year && m_writers == other.m_writers &&
           m_directors == other.m_directors && m_cast == other.m_cast &&
           m_categories == other.m_categories && m_ratingLabel == other.m_ratingLabel &&
           m_ratingIcon == other.m_ratingIcon && m_ratingSource == other.m_ratingSource;
  }

  bool operator!=(const Event& other) const { return !(*this == other); }

  uint32_t GetNext() const { return m_next; }
  void SetNext(uint32_t next) { m_next = next; }

  uint32_t GetChannel() const { return m_channel; }
  void SetChannel(uint32_t channel) { m_channel = channel; }

  uint32_t GetContent() const { return m_content; }
  void SetContent(uint32_t content) { m_content = content; }
  uint32_t GetGenreType() const { return m_content & 0xF0; }
  uint32_t GetGenreSubType() const { return m_content & 0x0F; }

  time_t GetStart() const { return m_start; }
  void SetStart(time_t start) { m_start = start; }

  time_t GetStop() const { return m_stop; }
  void SetStop(time_t stop) { m_stop = stop; }

  uint32_t GetStars() const { return m_stars; }
  void SetStars(uint32_t stars) { m_stars = stars; }

  uint32_t GetAge() const { return m_age; }
  void SetAge(uint32_t age) { m_age = age; }

  const std::string& GetRatingLabel() const { return m_ratingLabel; }
  void SetRatingLabel(const std::string& ratingLabel) { m_ratingLabel = ratingLabel; }

  const std::string& GetRatingIcon() const { return m_ratingIcon; }
  void SetRatingIcon(const std::string& ratingIcon) { m_ratingIcon = ratingIcon; }

  const std::string& GetRatingSource() const { return m_ratingSource; }
  void SetRatingSource(const std::string& ratingSource) { m_ratingSource = ratingSource; }

  int32_t GetSeason() const { return m_season; }
  void SetSeason(int32_t season) { m_season = season; }

  int32_t GetEpisode() const { return m_episode; }
  void SetEpisode(int32_t episode) { m_episode = episode; }

  int32_t GetPart() const { return m_part; }
  void SetPart(int32_t part) { m_part = part; }

  const std::string& GetTitle() const { return m_title; }
  void SetTitle(const std::string& title) { m_title = title; }

  const std::string& GetSubtitle() const { return m_subtitle; }
  void SetSubtitle(const std::string& subtitle) { m_subtitle = subtitle; }

  // TODO: Rename to GetDescription to match Recording
  const std::string& GetDesc() const { return m_desc; }
  void SetDesc(const std::string& desc) { m_desc = desc; }

  const std::string& GetSummary() const { return m_summary; }
  void SetSummary(const std::string& summary) { m_summary = summary; }

  const std::string& GetImage() const { return m_image; }
  void SetImage(const std::string& image) { m_image = image; }

  uint32_t GetRecordingId() const { return m_recordingId; }
  void SetRecordingId(uint32_t recordingId) { m_recordingId = recordingId; }

  const std::string& GetSeriesLink() const { return m_seriesLink; }
  void SetSeriesLink(const std::string& seriesLink) { m_seriesLink = seriesLink; }

  uint32_t GetYear() const { return m_year; }
  void SetYear(uint32_t year) { m_year = year; }

  const std::string& GetWriters() const { return m_writers; }
  void SetWriters(const std::vector<std::string>& writers);

  const std::string& GetDirectors() const { return m_directors; }
  void SetDirectors(const std::vector<std::string>& directors);

  const std::string& GetCast() const { return m_cast; }
  void SetCast(const std::vector<std::string>& cast);

  const std::string& GetCategories() const { return m_categories; }
  void SetCategories(const std::vector<std::string>& categories);

  const std::string& GetAired() const { return m_aired; }
  void SetAired(time_t aired);

private:
  uint32_t m_next;
  uint32_t m_channel;
  uint32_t m_content;
  time_t m_start;
  time_t m_stop;
  uint32_t m_stars; /* 1 - 5 */
  uint32_t m_age; /* years */
  int32_t m_season;
  int32_t m_episode;
  uint32_t m_part;
  std::string m_title;
  std::string m_subtitle; /* episode name */
  std::string m_desc;
  std::string m_summary;
  std::string m_image;
  uint32_t m_recordingId;
  std::string m_seriesLink;
  uint32_t m_year;
  std::string m_writers;
  std::string m_directors;
  std::string m_cast;
  std::string m_categories;
  std::string m_aired;
  std::string m_ratingLabel; // Label like 'PG' or 'FSK 12'
  std::string m_ratingIcon; // Path to graphic for the above label.
  std::string m_ratingSource; // Parental rating source.
};

} // namespace entity
} // namespace tvheadend
