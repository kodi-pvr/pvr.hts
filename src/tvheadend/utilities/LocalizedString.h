#pragma once

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

#include "../../client.h"

#include <string>

namespace tvheadend
{
namespace utilities
{
/**
     * Encapsulates an localized string.
     */
class LocalizedString
{
public:
  explicit LocalizedString(int stringId) : m_localizedString(XBMC->GetLocalizedString(stringId)) {}

  ~LocalizedString() { XBMC->FreeString(m_localizedString); }

  std::string Get() const
  {
    return m_localizedString ? std::string(m_localizedString) : std::string();
  }

private:
  LocalizedString() = delete;
  LocalizedString(const LocalizedString&) = delete;
  LocalizedString& operator=(const LocalizedString&) = delete;

  char* m_localizedString;
};
} // namespace utilities
} // namespace tvheadend
