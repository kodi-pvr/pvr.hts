#pragma once

/*
*      Copyright (C) 2005-2014 Team XBMC
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

#include "../HTSPTypes.h"
#include <string>

#include "xbmc_addon_types.h"

namespace tvheadend {

  /**
   * Represents the current addon settings
   */
  class Settings {
  public:

    // Default values.
    static const std::string DEFAULT_HOST;
    static const int         DEFAULT_HTTP_PORT;
    static const int         DEFAULT_HTSP_PORT;
    static const std::string DEFAULT_USERNAME;
    static const std::string DEFAULT_PASSWORD;
    static const int         DEFAULT_CONNECT_TIMEOUT;  // millisecs
    static const int         DEFAULT_RESPONSE_TIMEOUT; // millisecs
    static const bool        DEFAULT_TRACE_DEBUG;
    static const bool        DEFAULT_ASYNC_EPG;
    static const bool        DEFAULT_PRETUNER_ENABLED;
    static const int         DEFAULT_TOTAL_TUNERS;
    static const int         DEFAULT_PRETUNER_CLOSEDELAY; // secs
    static const int         DEFAULT_AUTOREC_MAXDIFF; // mins. Maximum difference between real and approximate start time for auto recordings
    static const int         DEFAULT_APPROX_TIME;     // 0..1 (0 = use a fixed start time, 1 = use an approximate start time for auto recordings)
    static const std::string DEFAULT_STREAMING_PROFILE;
    static const int         DEFAULT_DVR_PRIO;        // 0..4  (0 = max, 4 = min)
    static const int         DEFAULT_DVR_LIFETIME;    // 0..14 (0 = 1 day, 14 = forever)
    static const int         DEFAULT_DVR_DUBDETECT;   // 0..5  (0 = record all, 5 = limit to once a day)

    /**
     * Singleton getter for the instance
     */
    static Settings& GetInstance()
    {
      static Settings settings;
      return settings;
    }

    /**
     * Read all settings defined in settings.xml
     */
    void ReadSettings();

    /**
     * Set a value according to key definition in settings.xml
     */
    ADDON_STATUS SetSetting(const std::string &key, const void *value);

    /**
     * Getters for the settings values
     */
    std::string GetHostname() const { return m_strHostname; }
    const char *GetConstCharHostname() const { return m_strHostname.c_str(); }
    int         GetPortHTSP() const { return m_iPortHTSP; }
    int         GetPortHTTP() const { return m_iPortHTTP; }
    std::string GetUsername() const { return m_strUsername; }
    std::string GetPassword() const { return m_strPassword; }
    int         GetConnectTimeout() const { return m_iConnectTimeout; }
    int         GetResponseTimeout() const { return m_iResponseTimeout; }
    bool        GetTraceDebug() const { return m_bTraceDebug; }
    bool        GetAsyncEpg() const { return m_bAsyncEpg; }
    int         GetTotalTuners() const { return m_iTotalTuners; }
    int         GetPreTunerCloseDelay() const { return m_iPreTunerCloseDelay; }
    bool        GetAutorecApproxTime() const { return m_bAutorecApproxTime; }
    int         GetAutorecMaxDiff() const { return m_iAutorecMaxDiff; }
    std::string GetStreamingProfile() const { return m_strStreamingProfile; }
    int         GetDvrPriority() const { return m_iDvrPriority; }
    int         GetDvrDupdetect() const { return m_iDvrDupdetect; }
    int         GetDvrLifetime(bool asEnum = false) const;

  private:
    Settings()
    : m_strHostname(DEFAULT_HOST),
      m_iPortHTSP(DEFAULT_HTTP_PORT),
      m_iPortHTTP(DEFAULT_HTSP_PORT),
      m_strUsername(DEFAULT_USERNAME),
      m_strPassword(DEFAULT_PASSWORD),
      m_iConnectTimeout(DEFAULT_CONNECT_TIMEOUT),
      m_iResponseTimeout(DEFAULT_RESPONSE_TIMEOUT),
      m_bTraceDebug(DEFAULT_TRACE_DEBUG),
      m_bAsyncEpg(DEFAULT_ASYNC_EPG),
      m_bPretunerEnabled(DEFAULT_PRETUNER_ENABLED),
      m_iTotalTuners(DEFAULT_TOTAL_TUNERS),
      m_iPreTunerCloseDelay(DEFAULT_PRETUNER_CLOSEDELAY),
      m_bAutorecApproxTime(DEFAULT_APPROX_TIME),
      m_iAutorecMaxDiff(DEFAULT_AUTOREC_MAXDIFF),
      m_strStreamingProfile(DEFAULT_STREAMING_PROFILE),
      m_iDvrPriority(DEFAULT_DVR_PRIO),
      m_iDvrLifetime(DEFAULT_DVR_LIFETIME),
      m_iDvrDupdetect(DEFAULT_DVR_DUBDETECT) {}

    Settings(Settings const &) = delete;
    void operator=(Settings const &) = delete;

    /**
     * Setters
     */
    void SetHostname(const std::string& value) { m_strHostname = value; }
    void SetPortHTSP(int value) { m_iPortHTSP = value; }
    void SetPortHTTP(int value) { m_iPortHTTP = value; }
    void SetUsername(const std::string& value) { m_strUsername = value; }
    void SetPassword(const std::string& value) { m_strPassword = value; }
    void SetConnectTimeout(int value) { m_iConnectTimeout = value; }
    void SetResponseTimeout(int value) { m_iResponseTimeout = value; }
    void SetTraceDebug(bool value) { m_bTraceDebug = value; }
    void SetAsyncEpg(bool value) { m_bAsyncEpg = value; }
    void SetTotalTuners(int value) { m_iTotalTuners = value; }
    void SetPreTunerCloseDelay(int value) { m_iPreTunerCloseDelay = value; }
    void SetAutorecApproxTime(bool value) { m_bAutorecApproxTime = value; }
    void SetAutorecMaxDiff(int value) { m_iAutorecMaxDiff = value; }
    void SetStreamingProfile(const std::string &value) { m_strStreamingProfile = value; }
    void SetDvrPriority(int value) { m_iDvrPriority = value; }
    void SetDvrLifetime(int value) { m_iDvrLifetime = value; }
    void SetDvrDupdetect(int value) { m_iDvrDupdetect = value; }

    /**
     * Read/Set values according to definition in settings.xml
     */
    static std::string ReadStringSetting(const std::string &key, const std::string &def);
    static int         ReadIntSetting(const std::string &key, int def);
    static bool        ReadBoolSetting(const std::string &key, bool def);

    // @return ADDON_STATUS_OK if value has not changed, ADDON_STATUS_NEED_RESTART otherwise
    static ADDON_STATUS SetStringSetting(const std::string &oldValue, const void *newValue);
    static ADDON_STATUS SetIntSetting(int oldValue, const void *newValue);
    static ADDON_STATUS SetBoolSetting(bool oldValue, const void *newValue);

    std::string m_strHostname;
    int         m_iPortHTSP;
    int         m_iPortHTTP;
    std::string m_strUsername;
    std::string m_strPassword;
    int         m_iConnectTimeout;
    int         m_iResponseTimeout;
    bool        m_bTraceDebug;
    bool        m_bAsyncEpg;
    bool        m_bPretunerEnabled;
    int         m_iTotalTuners;
    int         m_iPreTunerCloseDelay;
    bool        m_bAutorecApproxTime;
    int         m_iAutorecMaxDiff;
    std::string m_strStreamingProfile;
    int         m_iDvrPriority;
    int         m_iDvrLifetime;
    int         m_iDvrDupdetect;
  };

}
