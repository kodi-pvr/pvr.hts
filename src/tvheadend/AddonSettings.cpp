/*
 *  Copyright (C) 2005-2022 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "AddonSettings.h"

#include "utilities/Logger.h"

#include "kodi/General.h"

using namespace tvheadend;
using namespace tvheadend::utilities;

namespace
{
constexpr bool DEFAULT_TRACE_DEBUG = false;

bool ReadBoolSetting(const std::string& key, bool def)
{
  bool value;
  if (kodi::addon::CheckSettingBoolean(key, value))
    return value;

  return def;
}

ADDON_STATUS SetBoolSetting(bool oldValue, const kodi::addon::CSettingValue& newValue)
{
  if (oldValue == newValue.GetBoolean())
    return ADDON_STATUS_OK;

  return ADDON_STATUS_NEED_RESTART;
}

} // unnamed namespace

AddonSettings::AddonSettings() : m_bTraceDebug(DEFAULT_TRACE_DEBUG)
{
  ReadSettings();
}

void AddonSettings::ReadSettings()
{
  SetTraceDebug(ReadBoolSetting("trace_debug", DEFAULT_TRACE_DEBUG));
}

ADDON_STATUS AddonSettings::SetSetting(const std::string& key,
                                       const kodi::addon::CSettingValue& value)
{
  if (key == "trace_debug")
  {
    return SetBoolSetting(GetTraceDebug(), value);
  }

  Logger::Log(LogLevel::LEVEL_ERROR, "AddonSettings::SetSetting - unknown setting '%s'",
              key.c_str());
  return ADDON_STATUS_UNKNOWN;
}
