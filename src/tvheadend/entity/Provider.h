/*
 *  Copyright (C) 2024 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "Entity.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>

namespace tvheadend::entity
{

class Provider;
using ProviderMapEntry = std::pair<int32_t, Provider>;
using Providers = std::map<int32_t, Provider>;

/**
 * Represents a provider
 */
class Provider : public Entity
{
public:
  Provider() = default;

  bool operator==(const Provider& other) const
  {
    return m_id == other.m_id && m_name == other.m_name;
  }

  bool operator!=(const Provider& other) const { return !(*this == other); }

  const std::string& GetName() const { return m_name; }
  void SetName(const std::string& name) { m_name = name; }

private:
  std::string m_name;
};
} // namespace tvheadend::entity
