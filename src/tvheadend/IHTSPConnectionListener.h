/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <mutex>

extern "C"
{
#include "libhts/htsmsg.h"
}

namespace tvheadend
{

/*
 * HTSP Connection Listener interface
 */
class IHTSPConnectionListener
{
public:
  virtual ~IHTSPConnectionListener() = default;

  virtual void Disconnected() = 0;
  virtual bool Connected(std::unique_lock<std::recursive_mutex>& lock) = 0;
  virtual bool ProcessMessage(const std::string& method, htsmsg_t* msg) = 0;
  virtual void ConnectionStateChange(const std::string& connectionString,
                                     PVR_CONNECTION_STATE newState,
                                     const std::string& message) = 0;
};

} // namespace tvheadend
