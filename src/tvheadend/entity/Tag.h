/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "../HTSPTypes.h"
#include "Channel.h"
#include "Entity.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tvheadend
{
namespace entity
{

class Tag;
typedef std::pair<uint32_t, Tag> TagMapEntry;
typedef std::map<uint32_t, Tag> Tags;

/**
 * Represents a channel tag
 */
class Tag : public Entity
{
public:
  Tag();

  bool operator==(const Tag& right);
  bool operator!=(const Tag& right);

  uint32_t GetIndex() const;
  void SetIndex(uint32_t index);

  const std::string& GetName() const;
  void SetName(const std::string& name);

  void SetIcon(const std::string& icon);

  const std::vector<uint32_t>& GetChannels() const;
  std::vector<uint32_t>& GetChannels();

  bool ContainsChannelType(channel_type_t eType, const Channels& channels) const;

private:
  uint32_t m_index;
  std::string m_name;
  std::string m_icon;
  std::vector<uint32_t> m_channels;
};

} // namespace entity
} // namespace tvheadend
