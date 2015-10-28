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

#include "../Tvheadend.h"

#include "Settings.h"

namespace tvheadend {

const std::string Settings::DEFAULT_HOST                = "127.0.0.1";
const int         Settings::DEFAULT_HTTP_PORT           = 9981;
const int         Settings::DEFAULT_HTSP_PORT           = 9982;
const std::string Settings::DEFAULT_USERNAME            = "";
const std::string Settings::DEFAULT_PASSWORD            = "";
const int         Settings::DEFAULT_CONNECT_TIMEOUT     = 10000; // millisecs
const int         Settings::DEFAULT_RESPONSE_TIMEOUT    = 5000;  // millisecs
const bool        Settings::DEFAULT_TRACE_DEBUG         = false;
const bool        Settings::DEFAULT_ASYNC_EPG           = false;
const int         Settings::DEFAULT_TOTAL_TUNERS        = 1;  // total tuners > 1 => predictive tuning active
const int         Settings::DEFAULT_PRETUNER_CLOSEDELAY = 10; // secs
const int         Settings::DEFAULT_AUTOREC_MAXDIFF     = 15; // mins. Maximum difference between real and approximate start time for auto recordings
const int         Settings::DEFAULT_APPROX_TIME         = 0;  // mins
const std::string Settings::DEFAULT_STREAMING_PROFILE   = "";
const bool        Settings::DEFAULT_STREAMING_CONFLICT  = false;

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
  bool bPretunerEnabled = ReadBoolSetting("pretuner_enabled", false);
  SetTotalTuners(bPretunerEnabled ? ReadIntSetting("total_tuners", DEFAULT_TOTAL_TUNERS) : 1);
  SetPreTunerCloseDelay(bPretunerEnabled ? ReadIntSetting("pretuner_closedelay", DEFAULT_PRETUNER_CLOSEDELAY) : 0);

  /* Auto recordings */
  SetAutorecApproxTime(ReadIntSetting("autorec_approxtime", DEFAULT_APPROX_TIME));
  SetAutorecMaxDiff(ReadIntSetting("autorec_maxdiff", DEFAULT_AUTOREC_MAXDIFF));

  /* Streaming */
  SetStreamingProfile(ReadStringSetting("streaming_profile", DEFAULT_STREAMING_PROFILE));

  /* Subscription conflict management */
  SetStreamingConflict(ReadBoolSetting("streaming_conflict", DEFAULT_STREAMING_CONFLICT));
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
    return SetIntSetting(GetConnectTimeout(), value);
  else if (key == "response_timeout")
    return SetIntSetting(GetResponseTimeout(), value);
  /* Debug */
  else if (key == "trace_debug")
    return SetBoolSetting(GetTraceDebug(), value);
  /* Data Transfer */
  else if (key == "epg_async")
    return SetBoolSetting(GetAsyncEpg(), value);
  /* Predictive Tuning */
  else if (key == "pretuner_enabled")
  {
    if (GetTotalTuners() > 1 && *(reinterpret_cast<const bool*>(value)))
      return ADDON_STATUS_OK;
    else
      return ADDON_STATUS_NEED_RESTART;
  }
  else if (key == "total_tuners")
    return SetIntSetting(GetTotalTuners(), value);
  else if (key == "pretuner_closedelay")
    return SetIntSetting(GetPreTunerCloseDelay(), value);
  /* Auto recordings */
  else if (key == "autorec_approxtime")
    return SetIntSetting(GetAutorecApproxTime(), value);
  else if (key == "autorec_maxdiff")
    return SetIntSetting(GetAutorecMaxDiff(), value);
  /* Streaming */
  else if (key == "streaming_profile")
    return SetStringSetting(GetStreamingProfile(), value);
  else if (key == "streaming_conflict")
    return SetBoolSetting(GetStreamingConflict(), value);
  else
  {
    tvherror("Settings::SetSetting - unknown setting '%s'", key.c_str());
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

ADDON_STATUS Settings::SetBoolSetting(int oldValue, const void *newValue)
{
  if (oldValue == *(reinterpret_cast<const bool *>(newValue)))
    return ADDON_STATUS_OK;

  return ADDON_STATUS_NEED_RESTART;
}

} // namespace tvheadend
