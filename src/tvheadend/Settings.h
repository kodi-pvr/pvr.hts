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

#include <string>

namespace tvheadend {

  /**
   * Represents the current addon settings
   */
  class Settings {
  public:

    /**
     * Singleton getter for the instance
     */
    static Settings& GetInstance()
    {
      static Settings settings;
      return settings;
    }

    std::string strHostname;
    int         iPortHTSP;
    int         iPortHTTP;
    std::string strUsername;
    std::string strPassword;
    int         iConnectTimeout;
    int         iResponseTimeout;
    bool        bTraceDebug;
    bool        bAsyncEpg;
    int         iTotalTuners;
    int         iPreTuneCloseDelay;
    bool        bAutorecApproxTime;
    int         iAutorecMaxDiff;

  private:
    Settings() { }
    Settings(Settings const &) = delete;
    void operator=(Settings const &) = delete;
  };

}