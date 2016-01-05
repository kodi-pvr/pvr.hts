/*
 *      Copyright (C) 2005-2011 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "utilities/Logger.h"
#include "../client.h"
#include "Settings.h"

using namespace tvheadend;
using namespace tvheadend::utilities;

const std::string Settings::DEFAULT_HOST                = "127.0.0.1";
const int         Settings::DEFAULT_HTTP_PORT           = 9981;
const int         Settings::DEFAULT_HTSP_PORT           = 9982;
const std::string Settings::DEFAULT_USERNAME            = "";
const std::string Settings::DEFAULT_PASSWORD            = "";
const int         Settings::DEFAULT_CONNECT_TIMEOUT     = 10000; // millisecs
const int         Settings::DEFAULT_RESPONSE_TIMEOUT    = 5000;  // millisecs
const bool        Settings::DEFAULT_TRACE_DEBUG         = false;
const bool        Settings::DEFAULT_ASYNC_EPG           = false;
const bool        Settings::DEFAULT_PRETUNER_ENABLED    = false;
const int         Settings::DEFAULT_TOTAL_TUNERS        = 1;  // total tuners > 1 => predictive tuning active
const int         Settings::DEFAULT_PRETUNER_CLOSEDELAY = 10; // secs
const int         Settings::DEFAULT_AUTOREC_MAXDIFF     = 15; // mins. Maximum difference between real and approximate start time for auto recordings
const int         Settings::DEFAULT_APPROX_TIME         = 0;  // don't use an approximate start time, use a fixed time instead for auto recordings
const std::string Settings::DEFAULT_STREAMING_PROFILE   = "";
const int         Settings::DEFAULT_DVR_PRIO            = DVR_PRIO_NORMAL;
const int         Settings::DEFAULT_DVR_LIFETIME        = 8; // enum 8 = 3 months
const int         Settings::DEFAULT_DVR_DUBDETECT       = DVR_AUTOREC_RECORD_ALL;

void Settings::ReadSettings()
{
  /* Connection */
  SetHostname(ReadStringSetting("host", DEFAULT_HOST));
  SetPortHTSP(ReadIntSetting("htsp_port", DEFAULT_HTSP_PORT));
  SetPortHTTP(ReadIntSetting("http_port", DEFAULT_HTTP_PORT));
  SetUsername(ReadStringSetting("user", DEFAULT_USERNAME));
  SetPassword(ReadStringSetting("pass", DEFAULT_PASSWORD));

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
  SetPreTunerCloseDelay(m_bPretunerEnabled ? ReadIntSetting("pretuner_closedelay", DEFAULT_PRETUNER_CLOSEDELAY) : 0);

  /* Auto recordings */
  SetAutorecApproxTime(ReadIntSetting("autorec_approxtime", DEFAULT_APPROX_TIME));
  SetAutorecMaxDiff(ReadIntSetting("autorec_maxdiff", DEFAULT_AUTOREC_MAXDIFF));

  /* Streaming */
  SetStreamingProfile(ReadStringSetting("streaming_profile", DEFAULT_STREAMING_PROFILE));

  /* Default dvr settings */
  SetDvrPriority(ReadIntSetting("dvr_priority", DEFAULT_DVR_PRIO));
  SetDvrLifetime(ReadIntSetting("dvr_lifetime", DEFAULT_DVR_LIFETIME));
  SetDvrDupdetect(ReadIntSetting("dvr_dubdetect", DEFAULT_DVR_DUBDETECT));
}

ADDON_STATUS Settings::SetSetting(const std::string &key, const void *value)
{
  /* Connection */
  if (key == "host")
    return SetStringSetting(GetHostname(), value);
  else if (key == "htsp_port")
    return SetIntSetting(GetPortHTSP(), value);
  else if (key == "http_port")
    return SetIntSetting(GetPortHTTP(), value);
  else if (key == "user")
    return SetStringSetting(GetUsername(), value);
  else if (key == "pass")
    return SetStringSetting(GetPassword(), value);
  else if (key == "connect_timeout")
  {
    if (GetConnectTimeout() == (*(reinterpret_cast<const int *>(value)) * 1000))
      return ADDON_STATUS_OK;
    else
      return ADDON_STATUS_NEED_RESTART;
  }
  else if (key == "response_timeout")
  {
    if (GetResponseTimeout() == (*(reinterpret_cast<const int *>(value)) * 1000))
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
  /* Streaming */
  else if (key == "streaming_profile")
    return SetStringSetting(GetStreamingProfile(), value);
  /* Default dvr settings */
  else if (key == "dvr_priority")
    return SetIntSetting(GetDvrPriority(), value);
  else if (key == "dvr_lifetime")
    return SetIntSetting(GetDvrLifetime(true), value);
  else if (key == "dvr_dubdetect")
    return SetIntSetting(GetDvrDupdetect(), value);
  else
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "Settings::SetSetting - unknown setting '%s'", key.c_str());
    return ADDON_STATUS_UNKNOWN;
  }
}

std::string Settings::ReadStringSetting(const std::string &key, const std::string &def)
{
  char value[1024];
  if (XBMC->GetSetting(key.c_str(), value))
    return value;

  return def;
}

int Settings::ReadIntSetting(const std::string &key, int def)
{
  int value;
  if (XBMC->GetSetting(key.c_str(), &value))
    return value;

  return def;
}

bool Settings::ReadBoolSetting(const std::string &key, bool def)
{
  bool value;
  if (XBMC->GetSetting(key.c_str(), &value))
    return value;

  return def;
}

ADDON_STATUS Settings::SetStringSetting(const std::string &oldValue, const void *newValue)
{
  if (oldValue == std::string(reinterpret_cast<const char *>(newValue)))
    return ADDON_STATUS_OK;

  return ADDON_STATUS_NEED_RESTART;
}

ADDON_STATUS Settings::SetIntSetting(int oldValue, const void *newValue)
{
  if (oldValue == *(reinterpret_cast<const int *>(newValue)))
    return ADDON_STATUS_OK;

  return ADDON_STATUS_NEED_RESTART;
}

ADDON_STATUS Settings::SetBoolSetting(bool oldValue, const void *newValue)
{
  if (oldValue == *(reinterpret_cast<const bool *>(newValue)))
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
      default:
        return DVR_RET_FOREVER;
    }
  }
}
