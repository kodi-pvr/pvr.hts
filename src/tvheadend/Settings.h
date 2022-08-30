/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "HTSPTypes.h"

#include "kodi/AddonBase.h"

#include <string>

namespace tvheadend
{

/**
   * Represents the current addon settings
   */
class Settings
{
public:
  // Default values.
  static const std::string DEFAULT_HOST;
  static const int DEFAULT_HTTP_PORT;
  static const int DEFAULT_HTSP_PORT;
  static const bool DEFAULT_USE_HTTPS;
  static const std::string DEFAULT_USERNAME;
  static const std::string DEFAULT_PASSWORD;
  static const std::string DEFAULT_WOL_MAC;
  static const int DEFAULT_CONNECT_TIMEOUT; // millisecs
  static const int DEFAULT_RESPONSE_TIMEOUT; // millisecs
  static const bool DEFAULT_TRACE_DEBUG;
  static const bool DEFAULT_ASYNC_EPG;
  static const bool DEFAULT_PRETUNER_ENABLED;
  static const int DEFAULT_TOTAL_TUNERS;
  static const int DEFAULT_PRETUNER_CLOSEDELAY; // secs
  static const int
      DEFAULT_AUTOREC_MAXDIFF; // mins. Maximum difference between real and approximate start time for auto recordings
  static const bool DEFAULT_AUTOREC_USE_REGEX;
  static const int
      DEFAULT_APPROX_TIME; // 0..1 (0 = use a fixed start time, 1 = use an approximate start time for auto recordings)
  static const std::string DEFAULT_STREAMING_PROFILE;
  static const bool DEFAULT_STREAMING_HTTP;
  static const int DEFAULT_DVR_PRIO; // any dvr_prio_t numeric value
  static const int DEFAULT_DVR_LIFETIME; // 0..15 (0 = 1 day, 15 = use backend setting)
  static const int DEFAULT_DVR_DUPDETECT; // 0..5  (0 = record all, 5 = limit to once a day)
  static const bool DEFAULT_DVR_PLAYSTATUS;
  static const int DEFAULT_STREAM_CHUNKSIZE; // KB
  static const bool DEFAULT_DVR_IGNORE_DUPLICATE_SCHEDULES;

  Settings()
    : m_strHostname(DEFAULT_HOST),
      m_iPortHTSP(DEFAULT_HTTP_PORT),
      m_iPortHTTP(DEFAULT_HTSP_PORT),
      m_bUseHTTPS(DEFAULT_USE_HTTPS),
      m_strUsername(DEFAULT_USERNAME),
      m_strPassword(DEFAULT_PASSWORD),
      m_strWolMac(DEFAULT_WOL_MAC),
      m_iConnectTimeout(DEFAULT_CONNECT_TIMEOUT),
      m_iResponseTimeout(DEFAULT_RESPONSE_TIMEOUT),
      m_bTraceDebug(DEFAULT_TRACE_DEBUG),
      m_bAsyncEpg(DEFAULT_ASYNC_EPG),
      m_bPretunerEnabled(DEFAULT_PRETUNER_ENABLED),
      m_iTotalTuners(DEFAULT_TOTAL_TUNERS),
      m_iPreTunerCloseDelay(DEFAULT_PRETUNER_CLOSEDELAY),
      m_iAutorecApproxTime(DEFAULT_APPROX_TIME),
      m_iAutorecMaxDiff(DEFAULT_AUTOREC_MAXDIFF),
      m_bAutorecUseRegEx(DEFAULT_AUTOREC_USE_REGEX),
      m_strStreamingProfile(DEFAULT_STREAMING_PROFILE),
      m_bUseHTTPStreaming(DEFAULT_STREAMING_HTTP),
      m_iDvrPriority(DEFAULT_DVR_PRIO),
      m_iDvrLifetime(DEFAULT_DVR_LIFETIME),
      m_iDvrDupdetect(DEFAULT_DVR_DUPDETECT),
      m_bDvrPlayStatus(DEFAULT_DVR_PLAYSTATUS),
      m_iStreamReadChunkSizeKB(DEFAULT_STREAM_CHUNKSIZE),
      m_bIgnoreDuplicateSchedules(DEFAULT_DVR_IGNORE_DUPLICATE_SCHEDULES)
  {
    ReadSettings();
  }

  /**
   * Set a value according to key definition in settings.xml
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
  bool GetTraceDebug() const { return m_bTraceDebug; }
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
  Settings(Settings const&) = delete;
  void operator=(Settings const&) = delete;

  /**
   * Read all settings defined in settings.xml
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
  void SetTraceDebug(bool value) { m_bTraceDebug = value; }
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
  static std::string ReadStringSetting(const std::string& key, const std::string& def);
  static int ReadIntSetting(const std::string& key, int def);
  static bool ReadBoolSetting(const std::string& key, bool def);

  // @return ADDON_STATUS_OK if value has not changed, ADDON_STATUS_NEED_RESTART otherwise
  static ADDON_STATUS SetStringSetting(const std::string& oldValue,
                                       const kodi::addon::CSettingValue& newValue);
  static ADDON_STATUS SetIntSetting(int oldValue, const kodi::addon::CSettingValue& newValue);
  static ADDON_STATUS SetBoolSetting(bool oldValue, const kodi::addon::CSettingValue& newValue);

  std::string m_strHostname;
  int m_iPortHTSP;
  int m_iPortHTTP;
  bool m_bUseHTTPS;
  std::string m_strUsername;
  std::string m_strPassword;
  std::string m_strWolMac;
  int m_iConnectTimeout;
  int m_iResponseTimeout;
  bool m_bTraceDebug;
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
