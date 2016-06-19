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

#include "Tag.h"
#include "Channel.h"
#include "../../Tvheadend.h"

using namespace tvheadend::entity;

Tag::Tag() :
m_index(0)
{
}

bool Tag::operator==(const Tag &right)
{
  return m_id == right.m_id &&
    m_index == right.m_index &&
    m_name == right.m_name &&
    m_icon == right.m_icon &&
    m_channels == right.m_channels;
}

bool Tag::operator!=(const Tag &right)
{
  return !(*this == right);
}

uint32_t Tag::GetIndex() const
{
  return m_index;
}

void Tag::SetIndex(uint32_t index)
{
  m_index = index;
}

const std::string& Tag::GetName() const
{
  return m_name;
}

void Tag::SetName(const std::string& name)
{
  m_name = name;
}

void Tag::SetIcon(const std::string& icon)
{
  m_icon = icon;
}

const std::vector<uint32_t>& Tag::GetChannels() const
{
  return m_channels;
}

std::vector<uint32_t>& Tag::GetChannels()
{
  return m_channels;
}

bool Tag::ContainsChannelType(channel_type_t eType) const
{
  std::vector<uint32_t>::const_iterator it;
  Channels::const_iterator cit;
  const Channels& channels = tvh->GetChannels();

  for (it = m_channels.begin(); it != m_channels.end(); ++it)
  {
    if ((cit = channels.find(*it)) != channels.end())
    {
      if (cit->second.GetType() == eType)
        return true;
    }
  }
  return false;
}
