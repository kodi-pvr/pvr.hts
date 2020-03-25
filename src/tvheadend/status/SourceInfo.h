/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <string>

namespace tvheadend
{
namespace status
{

/**
 * Represents information about the current service
 */
struct SourceInfo
{
  /**
   * The current adapter used
   */
  std::string si_adapter;

  /**
   * The network
   */
  std::string si_network;

  /**
   * The mux
   */
  std::string si_mux;

  /**
   * The service provider
   */
  std::string si_provider;

  /**
   * The service name
   */
  std::string si_service;

  /**
   * Clears the current status
   */
  void Clear()
  {
    si_adapter.clear();
    si_network.clear();
    si_mux.clear();
    si_provider.clear();
    si_service.clear();
  }
};

} // namespace status
} // namespace tvheadend
