/*
 *  Copyright (C) 2017-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "../../client.h"

#include <string>

namespace tvheadend
{
namespace utilities
{

/**
 * Encapsulates a localized string.
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
