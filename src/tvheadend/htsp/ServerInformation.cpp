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

#include "ServerInformation.h"
#include "p8-platform/util/StringUtils.h"
#include <algorithm>

using namespace tvheadend::htsp;

ServerInformation::ServerInformation() :
    m_serverName(""),
    m_serverVersion(""),
    m_htspVersion(0),
    m_webRoot("")
{
}

std::string ServerInformation::GetServerName() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_serverName;
}

void ServerInformation::SetServerName(const std::string &serverName)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_serverName = serverName;
}

std::string ServerInformation::GetServerVersion() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return StringUtils::Format("%s (HTSPv%d)",
                             m_serverVersion.c_str(), m_htspVersion);
}

void ServerInformation::SetServerVersion(const std::string &serverVersion)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_serverVersion = serverVersion;
}

uint32_t ServerInformation::GetHtspVersion() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_htspVersion;
}

void ServerInformation::SetHtspVersion(uint32_t htspVersion)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_htspVersion = htspVersion;
}

std::string ServerInformation::GetWebRoot() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_webRoot;
}

void ServerInformation::SetWebRoot(const std::string &webRoot)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_webRoot = webRoot;
}

bool ServerInformation::HasCapability(const std::string &capability) const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return std::find(m_capabilities.begin(), m_capabilities.end(), capability)
         != m_capabilities.end();
}

void ServerInformation::AddCapability(const std::string &capability)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_capabilities.push_back(capability);
}

std::vector<uint8_t> ServerInformation::GetChallenge() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_challenge;
}

void ServerInformation::SetChallenge(const void *challenge, size_t challengeLen)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_challenge.assign(reinterpret_cast<const uint8_t*>(challenge),
                     reinterpret_cast<const uint8_t*>(challenge) + challengeLen);
}
