/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Settings.h"

#include "utilities/Logger.h"

#include "kodi/General.h"

using namespace tvheadend;
using namespace tvheadend::utilities;

const std::string Settings::DEFAULT_HOST = "127.0.0.1";
const bool Settings::DEFAULT_USE_HTTPS = false;
const int Settings::DEFAULT_HTTP_PORT = 9981;
const int Settings::DEFAULT_HTSP_PORT = 9982;
const std::string Settings::DEFAULT_USERNAME = "";
const std::string Settings::DEFAULT_PASSWORD = "";
const std::string Settings::DEFAULT_WOL_MAC = "";
const int Settings::DEFAULT_CONNECT_TIMEOUT = 10000; // millisecs
const int Settings::DEFAULT_RESPONSE_TIMEOUT = 5000; // millisecs
const bool Settings::DEFAULT_TRACE_DEBUG = false;
const bool Settings::DEFAULT_ASYNC_EPG = true;
const bool Settings::DEFAULT_PRETUNER_ENABLED = false;
const int Settings::DEFAULT_TOTAL_TUNERS = 1; // total tuners > 1 => predictive tuning active
const int Settings::DEFAULT_PRETUNER_CLOSEDELAY = 10; // secs
const int Settings::DEFAULT_AUTOREC_MAXDIFF =
    15; // mins. Maximum difference between real and approximate start time for auto recordings
const bool Settings::DEFAULT_AUTOREC_USE_REGEX = false;
const int Settings::DEFAULT_APPROX_TIME =
    0; // don't use an approximate start time, use a fixed time instead for auto recordings
const std::string Settings::DEFAULT_STREAMING_PROFILE = "";
const bool Settings::DEFAULT_STREAMING_HTTP = false;
const int Settings::DEFAULT_DVR_PRIO = DVR_PRIO_NORMAL;
const int Settings::DEFAULT_DVR_LIFETIME = 15; // use backend setting
const int Settings::DEFAULT_DVR_DUPDETECT = DVR_AUTOREC_RECORD_ALL;
const bool Settings::DEFAULT_DVR_PLAYSTATUS = true;
const int Settings::DEFAULT_STREAM_CHUNKSIZE = 64; // KB
const bool Settings::DEFAULT_DVR_IGNORE_DUPLICATE_SCHEDULES = true;

void Settings::ReadSettings()
{
  /* Connection */
  SetHostname(ReadStringSetting("host", DEFAULT_HOST));
  SetPortHTSP(ReadIntSetting("htsp_port", DEFAULT_HTSP_PORT));
  SetPortHTTP(ReadIntSetting("http_port", DEFAULT_HTTP_PORT));
  SetUseHTTPS(ReadBoolSetting("https", DEFAULT_USE_HTTPS));
  SetUsername(ReadStringSetting("user", DEFAULT_USERNAME));
  SetPassword(ReadStringSetting("pass", DEFAULT_PASSWORD));
  SetWolMac(ReadStringSetting("wol_mac", DEFAULT_WOL_MAC));

  /* Note: Timeouts in settings UI are defined in seconds but we expect them to be in milliseconds. */
  SetConnectTimeout(ReadIntSetting("connect_timeout", DEFAULT_CONNECT_TIMEOUT / 1000) * 1000);
  SetResponseTimeout(ReadIntSetting("response_timeout", DEFAULT_RESPONSE_TIMEOUT / 1000) * 1000);

  /* Debug */
  SetTraceDebug(ReadBoolSetting("trace_debug", DEFAULT_TRACE_DEBUG));

  /* Data Transfer */
  SetAsyncEpg(ReadBoolSetting("epg_async", DEFAULT_ASYNC_EPG));

  /* Predictive Tuning */
  m_bPretunerEnabled = ReadBoolSetting("pretuner_enabled", DEFAULT_PRETUNER_ENABLED);
  SetTotalTuners(m_bPretunerEnabled ? ReadIntSetting("total_tuners", DEFAULT_TOTAL_TUNERS) : 1);
  SetPreTunerCloseDelay(
      m_bPretunerEnabled ? ReadIntSetting("pretuner_closedelay", DEFAULT_PRETUNER_CLOSEDELAY) : 0);

  /* Auto recordings */
  SetAutorecApproxTime(ReadIntSetting("autorec_approxtime", DEFAULT_APPROX_TIME));
  SetAutorecMaxDiff(ReadIntSetting("autorec_maxdiff", DEFAULT_AUTOREC_MAXDIFF));
  SetAutorecUseRegEx(ReadBoolSetting("autorec_use_regex", DEFAULT_AUTOREC_USE_REGEX));

  /* Streaming */
  SetStreamingProfile(ReadStringSetting("streaming_profile", DEFAULT_STREAMING_PROFILE));
  SetStreamingHTTP(ReadBoolSetting("streaming_http", DEFAULT_STREAMING_HTTP));

  /* Default dvr settings */
  SetDvrPriority(ReadIntSetting("dvr_priority", DEFAULT_DVR_PRIO));
  SetDvrLifetime(ReadIntSetting("dvr_lifetime2", DEFAULT_DVR_LIFETIME));
  SetDvrDupdetect(ReadIntSetting("dvr_dubdetect", DEFAULT_DVR_DUPDETECT));

  /* Sever based play status */
  SetDvrPlayStatus(ReadBoolSetting("dvr_playstatus", DEFAULT_DVR_PLAYSTATUS));

  /* Stream read chunk size */
  SetStreamReadChunkSizeKB(ReadIntSetting("stream_readchunksize", DEFAULT_STREAM_CHUNKSIZE));

  /* Scheduled recordings */
  SetIgnoreDuplicateSchedules(
      ReadBoolSetting("dvr_ignore_duplicates", DEFAULT_DVR_IGNORE_DUPLICATE_SCHEDULES));
}

ADDON_STATUS Settings::SetSetting(const std::string& key, const kodi::CSettingValue& value)
{
  /* Connection */
  if (key == "host")
    return SetStringSetting(GetHostname(), value);
  else if (key == "htsp_port")
    return SetIntSetting(GetPortHTSP(), value);
  else if (key == "http_port")
    return SetIntSetting(GetPortHTTP(), value);
  else if (key == "https")
    return SetBoolSetting(GetUseHTTPS(), value);
  else if (key == "user")
    return SetStringSetting(GetUsername(), value);
  else if (key == "pass")
    return SetStringSetting(GetPassword(), value);
  else if (key == "wol_mac")
    return SetStringSetting(GetWolMac(), value);
  else if (key == "connect_timeout")
  {
    if (GetConnectTimeout() == value.GetInt() * 1000)
      return ADDON_STATUS_OK;
    else
      return ADDON_STATUS_NEED_RESTART;
  }
  else if (key == "response_timeout")
  {
    if (GetResponseTimeout() == value.GetInt() * 1000)
      return ADDON_STATUS_OK;
    else
      return ADDON_STATUS_NEED_RESTART;
  }
  /* Debug */
  else if (key == "trace_debug")
    return SetBoolSetting(GetTraceDebug(), value);
  /* Data Transfer */
  else if (key == "epg_async")
    return SetBoolSetting(GetAsyncEpg(), value);
  /* Predictive Tuning */
  else if (key == "pretuner_enabled")
    return SetBoolSetting(m_bPretunerEnabled, value);
  else if (key == "total_tuners")
  {
    if (!m_bPretunerEnabled)
      return ADDON_STATUS_OK;
    else
      return SetIntSetting(GetTotalTuners(), value);
  }
  else if (key == "pretuner_closedelay")
  {
    if (!m_bPretunerEnabled)
      return ADDON_STATUS_OK;
    else
      return SetIntSetting(GetPreTunerCloseDelay(), value);
  }
  /* Auto recordings */
  else if (key == "autorec_approxtime")
    return SetIntSetting(GetAutorecApproxTime(), value);
  else if (key == "autorec_maxdiff")
    return SetIntSetting(GetAutorecMaxDiff(), value);
  else if (key == "autorec_use_regex")
    return SetBoolSetting(GetAutorecUseRegEx(), value);
  /* Streaming */
  else if (key == "streaming_profile")
    return SetStringSetting(GetStreamingProfile(), value);
  else if (key == "streaming_http")
    return SetBoolSetting(GetStreamingHTTP(), value);
  /* Default dvr settings */
  else if (key == "dvr_priority")
    return SetIntSetting(GetDvrPriority(), value);
  else if (key == "dvr_lifetime2")
    return SetIntSetting(GetDvrLifetime(true), value);
  else if (key == "dvr_dubdetect")
    return SetIntSetting(GetDvrDupdetect(), value);
  /* Server based play status */
  else if (key == "dvr_playstatus")
    return SetBoolSetting(GetDvrPlayStatus(), value);
  else if (key == "stream_readchunksize")
    return SetIntSetting(GetStreamReadChunkSize(), value);
  else if (key == "dvr_ignore_duplicates")
    return SetBoolSetting(GetIgnoreDuplicateSchedules(), value);
  else
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "Settings::SetSetting - unknown setting '%s'", key.c_str());
    return ADDON_STATUS_UNKNOWN;
  }
}

std::string Settings::ReadStringSetting(const std::string& key, const std::string& def)
{
  std::string value;
  if (kodi::CheckSettingString(key, value))
    return value;

  return def;
}

int Settings::ReadIntSetting(const std::string& key, int def)
{
  int value;
  if (kodi::CheckSettingInt(key, value))
    return value;

  return def;
}

bool Settings::ReadBoolSetting(const std::string& key, bool def)
{
  bool value;
  if (kodi::CheckSettingBoolean(key, value))
    return value;

  return def;
}

ADDON_STATUS Settings::SetStringSetting(const std::string& oldValue,
                                        const kodi::CSettingValue& newValue)
{
  if (oldValue == newValue.GetString())
    return ADDON_STATUS_OK;

  return ADDON_STATUS_NEED_RESTART;
}

ADDON_STATUS Settings::SetIntSetting(int oldValue, const kodi::CSettingValue& newValue)
{
  if (oldValue == newValue.GetInt())
    return ADDON_STATUS_OK;

  return ADDON_STATUS_NEED_RESTART;
}

ADDON_STATUS Settings::SetBoolSetting(bool oldValue, const kodi::CSettingValue& newValue)
{
  if (oldValue == newValue.GetBoolean())
    return ADDON_STATUS_OK;

  return ADDON_STATUS_NEED_RESTART;
}

int Settings::GetDvrLifetime(bool asEnum) const
{
  if (asEnum)
    return m_iDvrLifetime;
  else
  {
    switch (m_iDvrLifetime)
    {
      case 0:
        return DVR_RET_1DAY;
      case 1:
        return DVR_RET_3DAY;
      case 2:
        return DVR_RET_5DAY;
      case 3:
        return DVR_RET_1WEEK;
      case 4:
        return DVR_RET_2WEEK;
      case 5:
        return DVR_RET_3WEEK;
      case 6:
        return DVR_RET_1MONTH;
      case 7:
        return DVR_RET_2MONTH;
      case 8:
        return DVR_RET_3MONTH;
      case 9:
        return DVR_RET_6MONTH;
      case 10:
        return DVR_RET_1YEAR;
      case 11:
        return DVR_RET_2YEARS;
      case 12:
        return DVR_RET_3YEARS;
      case 13:
        return DVR_RET_SPACE;
      case 14:
        return DVR_RET_FOREVER;
      case 15:
      default:
        return DVR_RET_DVRCONFIG;
    }
  }
}
