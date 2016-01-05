#pragma once

/*
 *      Copyright (C) 2005-2015 Team Kodi
 *      http://kodi.tv
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
#include <vector>
#include <cstdint>
#include <cstdlib>

namespace tvheadend
{
  namespace htsp
  {

    /**
     * Holds information about a server (retrieved with the "hello" method)
     */
    class ServerInformation
    {
    public:

      ServerInformation();

      std::string GetServerName() const;
      void SetServerName(const std::string &serverName);

      std::string GetServerVersion() const;
      void SetServerVersion(const std::string &serverVersion);

      uint32_t GetHtspVersion() const;
      void SetHtspVersion(uint32_t htspVersion);

      std::string GetWebRoot() const;
      void SetWebRoot(const std::string &webRoot);

      bool HasCapability(const std::string &capability) const;
      void AddCapability(const std::string &capability);

      std::vector<uint8_t> GetChallenge() const;
      void SetChallenge(const void *challenge, size_t challengeLen);

    private:

      /**
       * The server name
       */
      std::string m_serverName;

      /**
       * The server version string
       */
      std::string m_serverVersion;

      /**
       * The HTSP version of the server
       */
      uint32_t m_htspVersion;

      /**
       * The web root for HTTP requests to the server
       */
      std::string m_webRoot;

      /**
       * The challenge (binary data) to use for authentication
       */
      std::vector<uint8_t> m_challenge;

      /**
       * The server capabilities
       */
      std::vector <std::string> m_capabilities;

    };

  }
}
