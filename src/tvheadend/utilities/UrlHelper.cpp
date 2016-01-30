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

#include "UrlHelper.h"
#include "p8-platform/util/StringUtils.h"

using namespace tvheadend;
using namespace tvheadend::utilities;

UrlHelper::UrlHelper(const tvheadend::Settings &settings,
                     const tvheadend::htsp::ServerInformation &serverInformation)
    : m_settings(settings),
      m_serverInformation(serverInformation)
{

}

std::string UrlHelper::GetImageUrl(const std::string &image) const
{
  // Image cache images
  if (StringUtils::StartsWith(image, "imagecache/"))
    return GetWebUrl("/" + image);

  // Absolute or empty URLs
  return image;
}

std::string UrlHelper::GetWebUrl(const std::string &resource) const
{
  return GetBaseUrl() + m_serverInformation.GetWebRoot() + resource;
}

std::string UrlHelper::GetBaseUrl() const
{
  // Generate the authentication string (user:pass@)
  std::string auth = m_settings.GetUsername();

  if (!(auth.empty() || m_settings.GetPassword().empty()))
    auth += ":" + m_settings.GetPassword();
  if (!auth.empty())
    auth += "@";

  return StringUtils::Format("http://%s%s:%d", auth.c_str(), m_settings.GetHostname().c_str(),
                             m_settings.GetPortHTTP());
}
