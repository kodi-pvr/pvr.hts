/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <string>
#include <vector>

namespace tvheadend
{

class Profile;
typedef std::vector<Profile> Profiles;

/**
 * Represents a single streaming profile
 */
class Profile
{
public:
  std::string GetUuid() const { return m_uuid; }
  void SetUuid(const std::string& uuid) { m_uuid = uuid; }

  std::string GetName() const { return m_name; }
  void SetName(const std::string& name) { m_name = name; }

  std::string GetComment() const { return m_comment; }
  void SetComment(const std::string& comment) { m_comment = comment; }

private:
  /**
   * The profile UUID
   */
  std::string m_uuid;

  /**
   * The profile name
   */
  std::string m_name;

  /**
   * The profile comment
   */
  std::string m_comment;
};

} // namespace tvheadend
