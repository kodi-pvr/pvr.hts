/*
 *  Copyright (C) 2005-2024 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "CustomTimerProperties.h"

#include "HTSPConnection.h"
#include "entity/AutoRecording.h"
#include "entity/Recording.h"
#include "entity/TimeRecording.h"
#include "utilities/Logger.h"

#include "kodi/addon-instance/pvr/General.h"

#include <string>

using namespace tvheadend;
using namespace tvheadend::entity;
using namespace tvheadend::utilities;

CustomTimerProperties::CustomTimerProperties(const std::vector<unsigned int>& propIds,
                                             HTSPConnection& conn,
                                             const Profiles& dvrConfigs)
  : m_propIds(propIds), m_conn(conn), m_dvrConfigs(dvrConfigs)
{
}

std::vector<kodi::addon::PVRSettingKeyValuePair> CustomTimerProperties::GetProperties(
    const tvheadend::entity::RecordingBase& rec) const
{
  std::vector<kodi::addon::PVRSettingKeyValuePair> customProps;
  GetCommonProperties(customProps, rec);

  if (customProps.size() < m_propIds.size())
    Logger::Log(LogLevel::LEVEL_ERROR, "Not all properties handled!");

  return customProps;
}

std::vector<kodi::addon::PVRSettingKeyValuePair> CustomTimerProperties::GetProperties(
    const tvheadend::entity::AutoRecording& autorec) const
{
  std::vector<kodi::addon::PVRSettingKeyValuePair> customProps;
  GetCommonProperties(customProps, autorec);

  for (unsigned int propId : m_propIds)
  {
    switch (propId)
    {
      case CUSTOM_PROP_ID_AUTOREC_BROADCASTTYPE:
      {
        // Broadcast type
        if (m_conn.GetProtocol() >= 39)
          customProps.emplace_back(CUSTOM_PROP_ID_AUTOREC_BROADCASTTYPE,
                                   autorec.GetBroadcastType());
        break;
      }
      default:
        break;
    }
  }

  if (customProps.size() < m_propIds.size())
    Logger::Log(LogLevel::LEVEL_ERROR, "Not all properties handled!");

  return customProps;
}

void CustomTimerProperties::GetCommonProperties(
    std::vector<kodi::addon::PVRSettingKeyValuePair>& props,
    const tvheadend::entity::RecordingBase& rec) const
{
  for (unsigned int propId : m_propIds)
  {
    switch (propId)
    {
      case CUSTOM_PROP_ID_DVR_CONFIGURATION:
      {
        // DVR configuration
        if (m_conn.GetProtocol() >= 40)
        {
          const int configId{GetDvrConfigurationId(rec.GetConfigUuid())};
          if (configId != -1)
            props.emplace_back(CUSTOM_PROP_ID_DVR_CONFIGURATION, configId);
        }
        break;
      }
      case CUSTOM_PROP_ID_DVR_COMMENT:
      {
        // User comment
        if (m_conn.GetProtocol() >= 42)
          props.emplace_back(CUSTOM_PROP_ID_DVR_COMMENT, rec.GetComment());

        break;
      }
      default:
        break;
    }
  }
}

const std::vector<kodi::addon::PVRSettingDefinition> CustomTimerProperties::GetSettingDefinitions()
    const
{
  std::vector<kodi::addon::PVRSettingDefinition> ret;

  for (unsigned int propId : m_propIds)
  {
    switch (propId)
    {
      case CUSTOM_PROP_ID_AUTOREC_BROADCASTTYPE:
      {
        // Broadcast type
        if (m_conn.GetProtocol() >= 39)
        {
          int defaultValue{0};
          const std::vector<kodi::addon::PVRTypeIntValue> broadcastTypeValues{
              GetPossibleValues(CUSTOM_PROP_ID_AUTOREC_BROADCASTTYPE, defaultValue)};
          ret.emplace_back(CreateSettingDefinition(CUSTOM_PROP_ID_AUTOREC_BROADCASTTYPE,
                                                   30600, // Broadcast type
                                                   broadcastTypeValues, defaultValue,
                                                   PVR_SETTING_READONLY_CONDITION_NONE));
        }
        break;
      }
      case CUSTOM_PROP_ID_DVR_CONFIGURATION:
      {
        // DVR configuration
        if (m_conn.GetProtocol() >= 40)
        {
          int defaultValue{0};
          const std::vector<kodi::addon::PVRTypeIntValue> dvrConfigValues{
              GetPossibleValues(CUSTOM_PROP_ID_DVR_CONFIGURATION, defaultValue)};
          if (dvrConfigValues.size() > 1)
          {
            ret.emplace_back(CreateSettingDefinition(
                CUSTOM_PROP_ID_DVR_CONFIGURATION,
                30457, // DVR configuration
                dvrConfigValues, defaultValue, PVR_SETTING_READONLY_CONDITION_TIMER_RECORDING));
          }
        }
        break;
      }
      case CUSTOM_PROP_ID_DVR_COMMENT:
      {
        // User comment
        if (m_conn.GetProtocol() >= 42)
        {
          std::string defaultValue;
          const std::vector<kodi::addon::PVRTypeStringValue> values{
              GetPossibleValues(CUSTOM_PROP_ID_DVR_COMMENT, defaultValue)};
          ret.emplace_back(CreateSettingDefinition(CUSTOM_PROP_ID_DVR_COMMENT,
                                                   30458, // Comment
                                                   values, defaultValue,
                                                   PVR_SETTING_READONLY_CONDITION_NONE));
        }
        break;
      }
      default:
        Logger::Log(LogLevel::LEVEL_ERROR, "Unknown property %u", propId);
        break;
    }
  }
  return ret;
}

const std::vector<kodi::addon::PVRTypeIntValue> CustomTimerProperties::GetPossibleValues(
    unsigned int propId, int& defaultValue) const
{
  switch (propId)
  {
    case CUSTOM_PROP_ID_AUTOREC_BROADCASTTYPE:
    {
      // Broadcast type
      if (m_conn.GetProtocol() >= 39)
      {
        static std::vector<kodi::addon::PVRTypeIntValue> broadcastTypeValues = {
            {DVR_AUTOREC_BTYPE_ALL, kodi::addon::GetLocalizedString(30601)},
            {DVR_AUTOREC_BTYPE_NEW_OR_UNKNOWN, kodi::addon::GetLocalizedString(30602)},
            {DVR_AUTOREC_BTYPE_REPEAT, kodi::addon::GetLocalizedString(30603)},
            {DVR_AUTOREC_BTYPE_NEW, kodi::addon::GetLocalizedString(30604)}};

        defaultValue = DVR_AUTOREC_BTYPE_ALL;
        return broadcastTypeValues;
      }
      break;
    }
    case CUSTOM_PROP_ID_DVR_CONFIGURATION:
    {
      // DVR configuration
      if (m_conn.GetProtocol() >= 40)
      {
        std::vector<kodi::addon::PVRTypeIntValue> dvrConfigValues;
        for (const auto& entry : m_dvrConfigs)
        {
          std::string name{entry.GetName()};
          if (name.empty())
          {
            name = kodi::addon::GetLocalizedString(30605); // (default profile)
            defaultValue = entry.GetId();
          }

          dvrConfigValues.emplace_back(entry.GetId(), name);
        }
        return dvrConfigValues;
      }
      break;
    }
    default:
      Logger::Log(LogLevel::LEVEL_ERROR, "Unknown property %u", propId);
      break;
  }
  return {};
}

const std::vector<kodi::addon::PVRTypeStringValue> CustomTimerProperties::GetPossibleValues(
    unsigned int propId, std::string& defaultValue) const
{
  switch (propId)
  {
    case CUSTOM_PROP_ID_DVR_COMMENT:
    {
      // User comment
      if (m_conn.GetProtocol() >= 42)
      {
        // Simple string prop, no pre-defined values; default is empty string.
        static const std::vector<kodi::addon::PVRTypeStringValue> values{};
        defaultValue = "";
        return values;
      }
      break;
    }
    default:
      Logger::Log(LogLevel::LEVEL_ERROR, "Unknown property %u", propId);
      break;
  }
  return {};
}

void CustomTimerProperties::AppendPropertiesToHTSPMessage(
    const std::vector<kodi::addon::PVRSettingKeyValuePair>& props, htsmsg_t* msg) const
{
  for (const auto& prop : props)
  {
    switch (prop.GetKey())
    {
      case CUSTOM_PROP_ID_AUTOREC_BROADCASTTYPE:
      {
        // Broadcast type
        htsmsg_add_u32(msg, "broadcastType", prop.GetIntValue());
        break;
      }
      case CUSTOM_PROP_ID_DVR_CONFIGURATION:
      {
        // DVR configuration
        for (const auto& config : m_dvrConfigs)
        {
          if (config.GetId() == prop.GetIntValue())
          {
            htsmsg_add_str(msg, "configName", config.GetUuid().c_str());
            break;
          }
        }
        break;
      }
      case CUSTOM_PROP_ID_DVR_COMMENT:
      {
        // User comment
        htsmsg_add_str(msg, "comment", prop.GetStringValue().c_str());
        break;
      }
      default:
        Logger::Log(LogLevel::LEVEL_ERROR, "Unknown property %u", prop.GetKey());
        break;
    }
  }
}

int CustomTimerProperties::GetDvrConfigurationId(const std::string& uuid) const
{
  if (m_conn.GetProtocol() >= 40 && m_dvrConfigs.size() > 1)
  {
    for (const auto& cfg : m_dvrConfigs)
    {
      if (cfg.GetUuid() == uuid)
        return cfg.GetId();
    }
  }
  return -1;
}

kodi::addon::PVRSettingDefinition CustomTimerProperties::CreateSettingDefinition(
    unsigned int settingId,
    int resourceId,
    const std::vector<kodi::addon::PVRTypeIntValue>& values,
    int defaultValue,
    uint64_t readonlyConditions) const
{
  kodi::addon::PVRSettingDefinition settingDef;
  settingDef.SetId(settingId);
  settingDef.SetName(kodi::addon::GetLocalizedString(resourceId));
  settingDef.SetType(PVR_SETTING_TYPE::INTEGER);
  settingDef.SetReadonlyConditions(readonlyConditions);

  kodi::addon::PVRIntSettingDefinition intSettingDef;
  intSettingDef.SetValues(std::move(values));
  intSettingDef.SetDefaultValue(defaultValue);

  settingDef.SetIntDefinition(intSettingDef);
  return settingDef;
}

kodi::addon::PVRSettingDefinition CustomTimerProperties::CreateSettingDefinition(
    unsigned int settingId,
    int resourceId,
    const std::vector<kodi::addon::PVRTypeStringValue>& values,
    const std::string& defaultValue,
    uint64_t readonlyConditions) const
{
  kodi::addon::PVRSettingDefinition settingDef;
  settingDef.SetId(settingId);
  settingDef.SetName(kodi::addon::GetLocalizedString(resourceId));
  settingDef.SetType(PVR_SETTING_TYPE::STRING);
  settingDef.SetReadonlyConditions(readonlyConditions);

  kodi::addon::PVRStringSettingDefinition stringSettingDef;
  stringSettingDef.SetValues(std::move(values));
  stringSettingDef.SetDefaultValue(defaultValue);
  stringSettingDef.SetAllowEmptyValue(true);

  settingDef.SetStringDefinition(stringSettingDef);
  return settingDef;
}
