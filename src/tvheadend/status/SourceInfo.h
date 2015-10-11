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

namespace tvheadend
{
  namespace status
  {
    /**
     * Represents information about the current service
     */
    struct SourceInfo
    {
      /**
       * The current adapter used
       */
      std::string si_adapter;

      /**
       * The network
       */
      std::string si_network;

      /**
       * The mux
       */
      std::string si_mux;

      /**
       * The service provider
       */
      std::string si_provider;

      /**
       * The service name
       */
      std::string si_service;

      /**
       * Clears the current status
       */
      void Clear()
      {
        si_adapter.clear();
        si_network.clear();
        si_mux.clear();
        si_provider.clear();
        si_service.clear();
      }
    };
  }
}
