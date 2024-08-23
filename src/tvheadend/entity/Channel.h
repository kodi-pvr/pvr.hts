/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "../HTSPTypes.h"
#include "Entity.h"

#include <cstdint>
#include <map>
#include <string>

namespace tvheadend
{
namespace entity
{

class Channel;
typedef std::pair<uint32_t, Channel> ChannelMapEntry;
typedef std::map<uint32_t, Channel> Channels;

/**
 * Represents a channel
 */
class Channel : public Entity
{
public:
  Channel() : m_num(0), m_numMinor(0), m_type(CHANNEL_TYPE_OTHER), m_caid(0) {}

  bool operator<(const Channel& right) const { return m_num < right.m_num; }

  bool operator==(const Channel& other) const
  {
    return m_id == other.m_id && m_num == other.m_num && m_numMinor == other.m_numMinor &&
           m_type == other.m_type && m_caid == other.m_caid && m_name == other.m_name &&
           m_icon == other.m_icon && m_providerUid == other.m_providerUid;
  }

  bool operator!=(const Channel& other) const { return !(*this == other); }

  uint32_t GetNum() const { return m_num; }
  void SetNum(uint32_t num) { m_num = num; }

  uint32_t GetNumMinor() const { return m_numMinor; }
  void SetNumMinor(uint32_t numMinor) { m_numMinor = numMinor; }

  uint32_t GetType() const { return m_type; }
  void SetType(uint32_t type) { m_type = type; }

  uint32_t GetCaid() const { return m_caid; }
  void SetCaid(uint32_t caid) { m_caid = caid; }

  const std::string& GetName() const { return m_name; }
  void SetName(const std::string& name) { m_name = name; }

  const std::string& GetIcon() const { return m_icon; }
  void SetIcon(const std::string& icon) { m_icon = icon; }

  int32_t GetProviderUid() const { return m_providerUid; }
  void SetProviderUid(int32_t providerUid) { m_providerUid = providerUid; }

private:
  uint32_t m_num;
  uint32_t m_numMinor;
  uint32_t m_type;
  uint32_t m_caid;
  std::string m_name;
  std::string m_icon;
  int32_t m_providerUid{PVR_PROVIDER_INVALID_UID};
};
} // namespace entity
} // namespace tvheadend
