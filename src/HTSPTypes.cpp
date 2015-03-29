/*
 *      Copyright (C) 2005-2015 Team XBMC
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

#include "Tvheadend.h"
#include "HTSPTypes.h"

namespace htsp
{

Tag::Tag(uint32_t id /*= 0*/) :
  m_dirty(false),
  m_id   (id),
  m_index(0)
{
}

bool Tag::operator==(const Tag &right)
{
  return m_id       == right.m_id &&
         m_index    == right.m_index &&
         m_name     == right.m_name &&
         m_icon     == right.m_icon &&
         m_channels == right.m_channels;
}

bool Tag::operator!=(const Tag &right)
{
  return !(*this == right);
}

bool Tag::IsDirty() const
{
  return m_dirty;
}

void Tag::SetDirty(bool bDirty)
{
  m_dirty = bDirty;
}

uint32_t Tag::GetId() const
{
   return m_id;
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

bool Tag::ContainsChannelType(bool bRadio) const
{
  std::vector<uint32_t>::const_iterator it;
  SChannels::const_iterator cit;
  const SChannels& channels = tvh->GetChannels();

  for (it = m_channels.begin(); it != m_channels.end(); ++it)
  {
    if ((cit = channels.find(*it)) != channels.end())
    {
      if (bRadio == cit->second.radio)
        return true;
    }
  }
  return false;
}

} // namespace htsp
