/*
 *  Copyright (C) 2005-2022 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "SettingsMigration.h"

#include "../HTSPTypes.h"

#include "kodi/General.h"

#include <algorithm>
#include <utility>
#include <vector>

using namespace tvheadend;
using namespace tvheadend::utilities;

namespace
{
// <setting name, default value> maps
const std::vector<std::pair<const char*, const char*>> stringMap = {
    {"host", "127.0.0.1"}, {"user", ""}, {"pass", ""}, {"wol_mac", ""}, {"streaming_profile", ""}};

const std::vector<std::pair<const char*, int>> intMap = {{"htsp_port", 9981},
                                                         {"http_port", 9982},
                                                         {"connect_timeout", 10000},
                                                         {"response_timeout", 5000},
                                                         {"total_tuners", 1},
                                                         {"pretuner_closedelay", 10},
                                                         {"autorec_approxtime", 0},
                                                         {"autorec_maxdiff", 15},
                                                         {"dvr_priority", DVR_PRIO_NORMAL},
                                                         {"dvr_lifetime2", 15},
                                                         {"dvr_dubdetect", DVR_AUTOREC_RECORD_ALL},
                                                         {"stream_readchunksize", 64}};

const std::vector<std::pair<const char*, bool>> boolMap = {{"https", false},
                                                           {"epg_async", true},
                                                           {"pretuner_enabled", false},
                                                           {"autorec_use_regex", false},
                                                           {"streaming_http", false},
                                                           {"dvr_playstatus", true},
                                                           {"dvr_ignore_duplicates", true}};

} // unnamed namespace

bool SettingsMigration::MigrateSettings(kodi::addon::IAddonInstance& target)
{
  std::string stringValue;
  bool boolValue{false};
  int intValue{0};

  if (target.CheckInstanceSettingString("kodi_addon_instance_name", stringValue) &&
      !stringValue.empty())
  {
    // Instance already has valid instance settings
    return false;
  }

  // Read pre-multi-instance settings from settings.xml, transfer to instance settings
  SettingsMigration mig(target);

  for (const auto& setting : stringMap)
    mig.MigrateStringSetting(setting.first, setting.second);

  for (const auto& setting : intMap)
    mig.MigrateIntSetting(setting.first, setting.second);

  for (const auto& setting : boolMap)
    mig.MigrateBoolSetting(setting.first, setting.second);

  if (mig.Changed())
  {
    // Set a title for the new instance settings
    std::string title;
    target.CheckInstanceSettingString("host", title);
    if (title.empty())
      title = "Migrated Add-on Config";

    target.SetInstanceSettingString("kodi_addon_instance_name", title);
    return true;
  }
  return false;
}

bool SettingsMigration::IsMigrationSetting(const std::string& key)
{
  return std::any_of(stringMap.cbegin(), stringMap.cend(),
                     [&key](const auto& entry) { return entry.first == key; }) ||
         std::any_of(intMap.cbegin(), intMap.cend(),
                     [&key](const auto& entry) { return entry.first == key; }) ||
         std::any_of(boolMap.cbegin(), boolMap.cend(),
                     [&key](const auto& entry) { return entry.first == key; });
}

void SettingsMigration::MigrateStringSetting(const char* key, const std::string& defaultValue)
{
  std::string value;
  if (kodi::addon::CheckSettingString(key, value) && value != defaultValue)
  {
    m_target.SetInstanceSettingString(key, value);
    m_changed = true;
  }
}

void SettingsMigration::MigrateIntSetting(const char* key, int defaultValue)
{
  int value;
  if (kodi::addon::CheckSettingInt(key, value) && value != defaultValue)
  {
    m_target.SetInstanceSettingInt(key, value);
    m_changed = true;
  }
}

void SettingsMigration::MigrateBoolSetting(const char* key, bool defaultValue)
{
  bool value;
  if (kodi::addon::CheckSettingBoolean(key, value) && value != defaultValue)
  {
    m_target.SetInstanceSettingBoolean(key, value);
    m_changed = true;
  }
}
