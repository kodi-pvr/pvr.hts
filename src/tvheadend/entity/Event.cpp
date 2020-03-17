/*
 *      Copyright (C) 2017 Team Kodi
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

#include "Event.h"

#include "kodi/xbmc_epg_types.h"
#include "p8-platform/util/StringUtils.h"

#include <ctime>

using namespace tvheadend::entity;

void Event::SetWriters(const std::vector<std::string>& writers)
{
  m_writers = StringUtils::Join(writers, EPG_STRING_TOKEN_SEPARATOR);
}

void Event::SetDirectors(const std::vector<std::string>& directors)
{
  m_directors = StringUtils::Join(directors, EPG_STRING_TOKEN_SEPARATOR);
}

void Event::SetCast(const std::vector<std::string>& cast)
{
  m_cast = StringUtils::Join(cast, EPG_STRING_TOKEN_SEPARATOR);
}

void Event::SetCategories(const std::vector<std::string>& categories)
{
  m_categories = StringUtils::Join(categories, EPG_STRING_TOKEN_SEPARATOR);
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
