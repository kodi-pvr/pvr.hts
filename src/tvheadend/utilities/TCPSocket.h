/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "kissnet/kissnet.hpp"

#include <memory>
#include <mutex>

namespace tvheadend
{
namespace utilities
{

class TCPSocket
{
public:
  TCPSocket() = delete;
  TCPSocket(const std::string& host, uint16_t port);

  virtual ~TCPSocket();

  bool Open(uint64_t iTimeoutMs = 0);

  void Shutdown();

  void Close();

  int64_t Read(void* data, size_t len, uint64_t iTimeoutMs = 0);

  int64_t Write(void* data, size_t len);

private:
  std::shared_ptr<kissnet::tcp_socket> GetSocket(bool bCreate = false);
  void ResetSocket();

  const kissnet::endpoint m_endpoint;
  std::shared_ptr<kissnet::tcp_socket> m_socket;
  std::recursive_mutex m_mutex;
};

} // namespace utilities
} // namespace tvheadend
