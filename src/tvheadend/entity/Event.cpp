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
