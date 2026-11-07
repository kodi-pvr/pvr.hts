/*
 *  Copyright (C) 2017-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Event.h"

#include "kodi/addon-instance/pvr/EPG.h"
#include "kodi/tools/StringUtils.h"

#include <ctime>

using namespace tvheadend::entity;

namespace
{
// FNV-1a, 64 bit
constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
constexpr uint64_t FNV_PRIME = 1099511628211ULL;

void HashBytes(uint64_t& h, const void* data, size_t len)
{
  const auto* p = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < len; ++i)
  {
    h ^= p[i];
    h *= FNV_PRIME;
  }
}

template<typename T>
void HashValue(uint64_t& h, const T& value)
{
  HashBytes(h, &value, sizeof(value));
}

void HashString(uint64_t& h, const std::string& str)
{
  // length prefix keeps adjacent strings apart
  const uint64_t len = str.size();
  HashValue(h, len);
  HashBytes(h, str.data(), str.size());
}
} // unnamed namespace

uint64_t Event::GetHash() const
{
  uint64_t h = FNV_OFFSET;
  HashValue(h, m_id);
  HashValue(h, m_next);
  HashValue(h, m_channel);
  HashValue(h, m_content);
  HashValue(h, m_start);
  HashValue(h, m_stop);
  HashValue(h, m_stars);
  HashValue(h, m_age);
  HashValue(h, m_season);
  HashValue(h, m_episode);
  HashValue(h, m_part);
  HashString(h, m_title);
  HashString(h, m_subtitle);
  HashString(h, m_desc);
  HashString(h, m_summary);
  HashString(h, m_image);
  HashValue(h, m_recordingId);
  HashString(h, m_seriesLink);
  HashValue(h, m_year);
  HashString(h, m_writers);
  HashString(h, m_directors);
  HashString(h, m_cast);
  HashString(h, m_categories);
  HashString(h, m_aired);
  HashString(h, m_ratingLabel);
  HashString(h, m_ratingIcon);
  HashString(h, m_ratingSource);
  HashValue(h, m_isnew);
  return h;
}

void Event::SetWriters(const std::vector<std::string>& writers)
{
  m_writers = kodi::tools::StringUtils::Join(writers, EPG_STRING_TOKEN_SEPARATOR);
}

void Event::SetDirectors(const std::vector<std::string>& directors)
{
  m_directors = kodi::tools::StringUtils::Join(directors, EPG_STRING_TOKEN_SEPARATOR);
}

void Event::SetCast(const std::vector<std::string>& cast)
{
  m_cast = kodi::tools::StringUtils::Join(cast, EPG_STRING_TOKEN_SEPARATOR);
}

void Event::SetCategories(const std::vector<std::string>& categories)
{
  m_categories = kodi::tools::StringUtils::Join(categories, EPG_STRING_TOKEN_SEPARATOR);
}

void Event::SetAired(time_t aired)
{
  if (aired > 0)
  {
    // convert to W3C date string
    std::tm* tm = std::localtime(&aired);
    char buffer[16];
    std::strftime(buffer, 16, "%Y-%m-%d", tm);
    m_aired = buffer;
  }
  else
  {
    m_aired.clear();
  }
}
