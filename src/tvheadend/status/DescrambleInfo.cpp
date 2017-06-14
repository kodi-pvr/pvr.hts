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

#include "DescrambleInfo.h"

#include "xbmc_pvr_types.h"

using namespace tvheadend::status;

DescrambleInfo::DescrambleInfo()
: m_pid(PVR_DESCRAMBLE_INFO_NOT_AVAILABLE),
  m_caid(PVR_DESCRAMBLE_INFO_NOT_AVAILABLE),
  m_provid(PVR_DESCRAMBLE_INFO_NOT_AVAILABLE),
  m_ecmTime(PVR_DESCRAMBLE_INFO_NOT_AVAILABLE),
  m_hops(PVR_DESCRAMBLE_INFO_NOT_AVAILABLE)
{
}

void DescrambleInfo::Clear()
{
  m_pid  = PVR_DESCRAMBLE_INFO_NOT_AVAILABLE;
  m_caid = PVR_DESCRAMBLE_INFO_NOT_AVAILABLE;
  m_provid = PVR_DESCRAMBLE_INFO_NOT_AVAILABLE;
  m_ecmTime = PVR_DESCRAMBLE_INFO_NOT_AVAILABLE;
  m_hops = PVR_DESCRAMBLE_INFO_NOT_AVAILABLE;
  m_cardSystem.clear();
  m_reader.clear();
  m_from.clear();
  m_protocol.clear();
}

int64_t DescrambleInfo::GetPid() const
{
  return m_pid;
}

void DescrambleInfo::SetPid(uint32_t pid)
{
  m_pid = pid;
}

int64_t DescrambleInfo::GetCaid() const
{
  return m_caid;
}

void DescrambleInfo::SetCaid(uint32_t caid)
{
  m_caid = caid;
}

int64_t DescrambleInfo::GetProvid() const
{
  return m_provid;
}

void DescrambleInfo::SetProvid(uint32_t provid)
{
  m_provid = provid;
}

int64_t DescrambleInfo::GetEcmTime() const
{
  return m_ecmTime;
}

void DescrambleInfo::SetEcmTime(uint32_t ecmTime)
{
  m_ecmTime = ecmTime;
}

int64_t DescrambleInfo::GetHops() const
{
  return m_hops;
}

void DescrambleInfo::SetHops(uint32_t hops)
{
  m_hops = hops;
}

std::string DescrambleInfo::GetCardSystem() const
{
  return m_cardSystem;
}

void DescrambleInfo::SetCardSystem(const std::string &cardSystem)
{
  m_cardSystem = cardSystem;
}

std::string DescrambleInfo::GetReader() const
{
  return m_reader;
}

void DescrambleInfo::SetReader(const std::string &reader)
{
  m_reader = reader;
}

std::string DescrambleInfo::GetFrom() const
{
  return m_from;
}

void DescrambleInfo::SetFrom(const std::string &from)
{
  m_from = from;
}

std::string DescrambleInfo::GetProtocol() const
{
  return m_protocol;
}

void DescrambleInfo::SetProtocol(const std::string &protocol)
{
  m_protocol = protocol;
}
