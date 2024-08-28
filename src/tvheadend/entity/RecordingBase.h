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
#include <string>

namespace tvheadend::entity
{

class RecordingBase : public Entity
{
protected:
  RecordingBase() = default;

  bool operator==(const RecordingBase& right)
  {
    return Entity::operator==(right) && m_enabled == right.m_enabled &&
           m_lifetime == right.m_lifetime && m_priority == right.m_priority &&
           m_title == right.m_title && m_channel == right.m_channel &&
           m_configUuid == right.m_configUuid && m_comment == right.m_comment;
  }

  bool operator!=(const RecordingBase& right) { return !(*this == right); }

public:
  bool IsEnabled() const { return m_enabled != 0; }
  void SetEnabled(uint32_t enabled) { m_enabled = enabled; }

  int GetLifetime() const;
  void SetLifetime(uint32_t lifetime) { m_lifetime = lifetime; }

  uint32_t GetPriority() const { return m_priority; }
  void SetPriority(uint32_t priority) { m_priority = priority; }

  const std::string& GetTitle() const { return m_title; }
  void SetTitle(const std::string& title) { m_title = title; }

  uint32_t GetChannel() const { return m_channel; }
  void SetChannel(uint32_t channel) { m_channel = channel; }

  const std::string& GetConfigUuid() const { return m_configUuid; }
  void SetConfigUuid(const std::string& uuid) { m_configUuid = uuid; }

  const std::string& GetComment() const { return m_comment; }
  void SetComment(const std::string& comment) { m_comment = comment; }

private:
  uint32_t m_enabled{0}; // If [time|auto]rec entry is enabled (activated).
  uint32_t m_lifetime{0}; // Lifetime (in days).
  uint32_t m_priority{DVR_PRIO_DEFAULT}; // Priority.
  std::string m_title; // Title (pattern) for the recording files.
  uint32_t m_channel{0}; // Channel ID.
  std::string m_configUuid; // DVR configuration UUID.
  std::string m_comment; // user supplied comment
};

} // namespace tvheadend::entity
