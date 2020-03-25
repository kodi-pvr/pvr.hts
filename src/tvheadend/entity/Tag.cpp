/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Tag.h"

using namespace tvheadend;
using namespace tvheadend::entity;

Tag::Tag() : m_index(0)
{
}

bool Tag::operator==(const Tag& right)
{
  return m_id == right.m_id && m_index == right.m_index && m_name == right.m_name &&
         m_icon == right.m_icon && m_channels == right.m_channels;
}

bool Tag::operator!=(const Tag& right)
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

bool Tag::ContainsChannelType(channel_type_t eType, const Channels& channels) const
{
  for (const auto& channel : m_channels)
  {
    const auto it = channels.find(channel);
    if (it != channels.end())
    {
      if (it->second.GetType() == eType)
        return true;
    }
  }
  return false;
}
