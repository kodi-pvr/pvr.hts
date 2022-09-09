/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "addon.h"

#include "Tvheadend.h"
#include "tvheadend/AddonSettings.h"
#include "tvheadend/utilities/Logger.h"
#include "tvheadend/utilities/SettingsMigration.h"

using namespace tvheadend;
using namespace tvheadend::utilities;

ADDON_STATUS CHTSAddon::Create()
{
  /* Init settings */
  m_settings.reset(new AddonSettings());

  /* Configure the logger */
  Logger::GetInstance().SetImplementation(
      [this](LogLevel level, const char* message)
      {
        /* Don't log trace messages unless told so */
        if (level == LogLevel::LEVEL_TRACE && !m_settings->GetTraceDebug())
          return;

        /* Convert the log level */
        ADDON_LOG addonLevel;

        switch (level)
        {
          case LogLevel::LEVEL_FATAL:
            addonLevel = ADDON_LOG::ADDON_LOG_FATAL;
            break;
          case LogLevel::LEVEL_ERROR:
            addonLevel = ADDON_LOG::ADDON_LOG_ERROR;
            break;
          case LogLevel::LEVEL_WARNING:
            addonLevel = ADDON_LOG::ADDON_LOG_WARNING;
            break;
          case LogLevel::LEVEL_INFO:
            addonLevel = ADDON_LOG::ADDON_LOG_INFO;
            break;
          default:
            addonLevel = ADDON_LOG::ADDON_LOG_DEBUG;
            break;
        }

        kodi::Log(addonLevel, "%s", message);
      });

  Logger::Log(LogLevel::LEVEL_INFO, "starting PVR client");

  return ADDON_STATUS_OK;
}

ADDON_STATUS CHTSAddon::SetSetting(const std::string& settingName,
                                   const kodi::addon::CSettingValue& settingValue)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  return m_settings->SetSetting(settingName, settingValue);
}

ADDON_STATUS CHTSAddon::CreateInstance(const kodi::addon::IInstanceInfo& instance,
                                       KODI_ADDON_INSTANCE_HDL& hdl)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (instance.IsType(ADDON_INSTANCE_PVR))
  {
    Logger::Log(LogLevel::LEVEL_DEBUG, "%s: Creating PVR-Client instance", __FUNCTION__);

    /* Connect to TVHeadend backend */
    CTvheadend* client = new CTvheadend(instance);

    // Try to migrate settings from a pre-multi-instance setup
    if (SettingsMigration::MigrateSettings(*client))
    {
      // Initial client operated on old/incomplete settings
      delete client;
      client = new CTvheadend(instance);
    }

    client->Start();
    hdl = client;

    return ADDON_STATUS_OK;
  }

  return ADDON_STATUS_UNKNOWN;
}

ADDONCREATOR(CHTSAddon)
