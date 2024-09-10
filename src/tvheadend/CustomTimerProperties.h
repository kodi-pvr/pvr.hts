/*
 *  Copyright (C) 2005-2024 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "Profile.h"

#include <vector>

extern "C"
{
#include "libhts/htsmsg.h"
}

namespace kodi::addon
{
class PVRSettingDefinition;
class PVRSettingKeyValuePair;
class PVRTypeIntValue;
class PVRTypeStringValue;
} // namespace kodi::addon

namespace tvheadend
{
class HTSPConnection;

namespace entity
{
class AutoRecording;
class RecordingBase;
} // namespace entity

// custom property ids
constexpr unsigned int CUSTOM_PROP_ID_AUTOREC_BROADCASTTYPE{1};
constexpr unsigned int CUSTOM_PROP_ID_DVR_CONFIGURATION{2};
constexpr unsigned int CUSTOM_PROP_ID_DVR_COMMENT{3};

class CustomTimerProperties
{
public:
  CustomTimerProperties(const std::vector<unsigned int>& propIds,
                        HTSPConnection& conn,
                        const tvheadend::Profiles& dvrConfigs);
  virtual ~CustomTimerProperties() = default;

  // Get custom props for all timers
  std::vector<kodi::addon::PVRSettingKeyValuePair> GetProperties(
      const tvheadend::entity::RecordingBase& rec) const;

  // Get custom props for Autorecs, inlcuding props for all timers
  std::vector<kodi::addon::PVRSettingKeyValuePair> GetProperties(
      const tvheadend::entity::AutoRecording& autorec) const;

  // Get setting definitions
  const std::vector<kodi::addon::PVRSettingDefinition> GetSettingDefinitions() const;

  // Append given props to given HTSP message
  void AppendPropertiesToHTSPMessage(const std::vector<kodi::addon::PVRSettingKeyValuePair>& props,
                                     htsmsg_t* msg) const;

private:
  const std::vector<kodi::addon::PVRTypeIntValue> GetPossibleValues(unsigned int propId,
                                                                    int& defaultValue) const;
  const std::vector<kodi::addon::PVRTypeStringValue> GetPossibleValues(
      unsigned int propId, std::string& defaultValue) const;
  kodi::addon::PVRSettingDefinition CreateSettingDefinition(
      unsigned int settingId,
      int resourceId,
      const std::vector<kodi::addon::PVRTypeIntValue>& values,
      int defaultValue,
      uint64_t readonlyConditions) const;
  kodi::addon::PVRSettingDefinition CreateSettingDefinition(
      unsigned int settingId,
      int resourceId,
      const std::vector<kodi::addon::PVRTypeStringValue>& values,
      const std::string& defaultValue,
      uint64_t readonlyConditions) const;

  int GetDvrConfigurationId(const std::string& uuid) const;
  void GetCommonProperties(std::vector<kodi::addon::PVRSettingKeyValuePair>& props,
                           const tvheadend::entity::RecordingBase& rec) const;

  const std::vector<unsigned int> m_propIds;
  const HTSPConnection& m_conn;
  const tvheadend::Profiles& m_dvrConfigs;
};

} // namespace tvheadend
