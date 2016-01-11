#pragma once

/*
 *      Copyright (C) 2005-2016 Team Kodi
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

#include <cstdint>
#include <string>

namespace tvheadend
{
  namespace status
  {

    /**
     * Contains information about the descrambler used
     */
    class DescrambleInfo
    {
    public:

      uint32_t GetPid() const;
      void SetPid(uint32_t pid);

      uint32_t GetCaid() const;
      void SetCaid(uint32_t caid);

      uint32_t GetProvid() const;
      void SetProvid(uint32_t provid);

      uint32_t GetEcmTime() const;
      void SetEcmTime(uint32_t ecmTime);

      uint32_t GetHops() const;
      void SetHops(uint32_t hops);

      std::string GetCardSystem() const;
      void SetCardSystem(const std::string &cardSystem);

      std::string GetReader() const;
      void SetReader(const std::string &reader);

      std::string GetFrom() const;
      void SetFrom(const std::string &from);

      std::string GetProtocol() const;
      void SetProtocol(const std::string &protocol);

    private:
      uint32_t m_pid;
      uint32_t m_caid;
      uint32_t m_provid;
      uint32_t m_ecmTime;
      uint32_t m_hops;
      std::string m_cardSystem;
      std::string m_reader;
      std::string m_from;
      std::string m_protocol;

    };

  }
}
