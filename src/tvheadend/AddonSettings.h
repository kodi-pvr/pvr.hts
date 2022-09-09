/*
 *  Copyright (C) 2005-2022 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "kodi/AddonBase.h"

#include <string>

namespace tvheadend
{
/**
   * Represents the current addon settings
   */
class AddonSettings
{
public:
  AddonSettings();

  /**
   * Set a value according to key definition in settings.xml
   */
  ADDON_STATUS SetSetting(const std::string& key, const kodi::addon::CSettingValue& value);

  /**
   * Getters for the settings values
   */
  bool GetTraceDebug() const { return m_bTraceDebug; }

private:
  AddonSettings(const AddonSettings&) = delete;
  void operator=(const AddonSettings&) = delete;

  /**
   * Read all settings defined in settings.xml
   */
  void ReadSettings();

  /**
   * Setters
   */
  void SetTraceDebug(bool value) { m_bTraceDebug = value; }

  bool m_bTraceDebug{false};
};

} // namespace tvheadend
