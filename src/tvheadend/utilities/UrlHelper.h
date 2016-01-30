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

#include "../Settings.h"
#include "../htsp/ServerInformation.h"
#include <string>

namespace tvheadend
{
  namespace utilities
  {

    /**
     * Helper for generating URLs to resources on the server
     */
    class UrlHelper
    {

    public:
      /**
       * Constructor for injecting dependencies
       */
      UrlHelper(const tvheadend::Settings &settings,
                const tvheadend::htsp::ServerInformation &serverInformation);

      /**
       * Returns the absolute URL to the specified image
       */
      std::string GetImageUrl(const std::string &image) const;

    private:

      /**
       * Returns the absolute URL to the specified resource
       */
      std::string GetWebUrl(const std::string &resource) const;

      /**
       * Returns the base URL for all resources
       */
      std::string GetBaseUrl() const;

      const tvheadend::Settings &m_settings;
      const tvheadend::htsp::ServerInformation &m_serverInformation;

    };
  }
}
