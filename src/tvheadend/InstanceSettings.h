/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
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
   * Represents the current addon instance settings
   */
class InstanceSettings
{
public:
  explicit InstanceSettings(kodi::addon::IAddonInstance& instance);

  /**
   * Set a value according to key definition in instance-settings.xml
   */
  ADDON_STATUS SetSetting(const std::string& key, const kodi::addon::CSettingValue& value);

  /**
   * Getters for the settings values
   */
  std::string GetHostname() const { return m_strHostname; }
  const char* GetConstCharHostname() const { return m_strHostname.c_str(); }
  int GetPortHTSP() const { return m_iPortHTSP; }
  int GetPortHTTP() const { return m_iPortHTTP; }
  bool GetUseHTTPS() const { return m_bUseHTTPS; }
  std::string GetUsername() const { return m_strUsername; }
  std::string GetPassword() const { return m_strPassword; }
  std::string GetWolMac() const { return m_strWolMac; }
  int GetConnectTimeout() const { return m_iConnectTimeout; }
  int GetResponseTimeout() const { return m_iResponseTimeout; }
  bool GetAsyncEpg() const { return m_bAsyncEpg; }
  int GetTotalTuners() const { return m_iTotalTuners; }
  int GetPreTunerCloseDelay() const { return m_iPreTunerCloseDelay; }
  int GetAutorecApproxTime() const { return m_iAutorecApproxTime; }
  int GetAutorecMaxDiff() const { return m_iAutorecMaxDiff; }
  bool GetAutorecUseRegEx() const { return m_bAutorecUseRegEx; }
  std::string GetStreamingProfile() const { return m_strStreamingProfile; }
  bool GetStreamingHTTP() const { return m_bUseHTTPStreaming; }
  int GetDvrPriority() const { return m_iDvrPriority; }
  int GetDvrDupdetect() const { return m_iDvrDupdetect; }
  int GetDvrLifetime(bool asEnum = false) const;
  bool GetDvrPlayStatus() const { return m_bDvrPlayStatus; }
  int GetStreamReadChunkSize() const { return m_iStreamReadChunkSizeKB; }
  bool GetIgnoreDuplicateSchedules() const { return m_bIgnoreDuplicateSchedules; }

private:
  InstanceSettings(const InstanceSettings&) = delete;
  void operator=(const InstanceSettings&) = delete;

  /**
   * Read all settings defined in instance-settings.xml
   */
  void ReadSettings();

  /**
   * Setters
   */
  void SetHostname(const std::string& value) { m_strHostname = value; }
  void SetPortHTSP(int value) { m_iPortHTSP = value; }
  void SetPortHTTP(int value) { m_iPortHTTP = value; }
  void SetUseHTTPS(bool value) { m_bUseHTTPS = value; }
  void SetUsername(const std::string& value) { m_strUsername = value; }
  void SetPassword(const std::string& value) { m_strPassword = value; }
  void SetWolMac(const std::string& value) { m_strWolMac = value; }
  void SetConnectTimeout(int value) { m_iConnectTimeout = value; }
  void SetResponseTimeout(int value) { m_iResponseTimeout = value; }
  void SetAsyncEpg(bool value) { m_bAsyncEpg = value; }
  void SetTotalTuners(int value) { m_iTotalTuners = value; }
  void SetPreTunerCloseDelay(int value) { m_iPreTunerCloseDelay = value; }
  void SetAutorecApproxTime(int value) { m_iAutorecApproxTime = value; }
  void SetAutorecMaxDiff(int value) { m_iAutorecMaxDiff = value; }
  void SetAutorecUseRegEx(bool value) { m_bAutorecUseRegEx = value; }
  void SetStreamingProfile(const std::string& value) { m_strStreamingProfile = value; }
  void SetStreamingHTTP(bool value) { m_bUseHTTPStreaming = value; }
  void SetDvrPriority(int value) { m_iDvrPriority = value; }
  void SetDvrLifetime(int value) { m_iDvrLifetime = value; }
  void SetDvrDupdetect(int value) { m_iDvrDupdetect = value; }
  void SetDvrPlayStatus(bool value) { m_bDvrPlayStatus = value; }
  void SetStreamReadChunkSizeKB(int value) { m_iStreamReadChunkSizeKB = value; }
  void SetIgnoreDuplicateSchedules(bool value) { m_bIgnoreDuplicateSchedules = value; }

  /**
   * Read/Set values according to definition in settings.xml
   */
  std::string ReadStringSetting(const std::string& key, const std::string& def) const;
  int ReadIntSetting(const std::string& key, int def) const;
  bool ReadBoolSetting(const std::string& key, bool def) const;

  // @return ADDON_STATUS_OK if value has not changed, ADDON_STATUS_NEED_RESTART otherwise
  ADDON_STATUS SetStringSetting(const std::string& oldValue,
                                const kodi::addon::CSettingValue& newValue);
  ADDON_STATUS SetIntSetting(int oldValue, const kodi::addon::CSettingValue& newValue);
  ADDON_STATUS SetBoolSetting(bool oldValue, const kodi::addon::CSettingValue& newValue);

  kodi::addon::IAddonInstance& m_instance;

  std::string m_strHostname;
  int m_iPortHTSP;
  int m_iPortHTTP;
  bool m_bUseHTTPS;
  std::string m_strUsername;
  std::string m_strPassword;
  std::string m_strWolMac;
  int m_iConnectTimeout;
  int m_iResponseTimeout;
  bool m_bAsyncEpg;
  bool m_bPretunerEnabled;
  int m_iTotalTuners;
  int m_iPreTunerCloseDelay;
  bool m_iAutorecApproxTime;
  int m_iAutorecMaxDiff;
  bool m_bAutorecUseRegEx;
  std::string m_strStreamingProfile;
  bool m_bUseHTTPStreaming;
  int m_iDvrPriority;
  int m_iDvrLifetime;
  int m_iDvrDupdetect;
  bool m_bDvrPlayStatus;
  int m_iStreamReadChunkSizeKB;
  bool m_bIgnoreDuplicateSchedules;
};

} // namespace tvheadend
