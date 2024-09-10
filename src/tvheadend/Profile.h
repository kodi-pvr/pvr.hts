/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

namespace tvheadend
{

class Profile;
typedef std::vector<Profile> Profiles;

/**
 * Represents a single streaming profile or a single DVR configuration
 */
class Profile
{
public:
  Profile() : m_id(GetNextIntId()) {}

  uint32_t GetId() const { return m_id; }
  void SetId(uint32_t profileId) { m_id = profileId; }

  std::string GetUuid() const { return m_uuid; }
  void SetUuid(const std::string& uuid) { m_uuid = uuid; }

  std::string GetName() const { return m_name; }
  void SetName(const std::string& name) { m_name = name; }

  std::string GetComment() const { return m_comment; }
  void SetComment(const std::string& comment) { m_comment = comment; }

private:
  static unsigned int GetNextIntId()
  {
    static unsigned int intId = 0;
    return ++intId;
  }

  uint32_t m_id{0};
  std::string m_uuid;
  std::string m_name;
  std::string m_comment;
};

} // namespace tvheadend
