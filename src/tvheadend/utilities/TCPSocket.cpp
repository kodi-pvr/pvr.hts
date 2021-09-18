/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "TCPSocket.h"

#include <chrono>

namespace
{

uint64_t MillisecondsSinceEpoch()
{
  const auto duration = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // unnamed namespace

using namespace tvheadend::utilities;

TCPSocket::TCPSocket(const std::string& host, uint16_t port) : m_endpoint(host, port)
{
}

TCPSocket::~TCPSocket()
{
  Close();
}

bool TCPSocket::Open(uint64_t iTimeoutMs /*= 0*/)
{
  try
  {
    auto socket = GetSocket(true);

    int status = socket->connect(iTimeoutMs).value; // connect non-blocking

    if (status == kissnet::socket_status::valid)
      socket->set_tcp_no_delay(true);

    return status == kissnet::socket_status::valid;
  }
  catch (std::runtime_error const&)
  {
    ResetSocket();
    return false;
  }
}

void TCPSocket::Shutdown()
{
  auto socket = GetSocket();
  if (!socket)
    return;

  try
  {
    socket->shutdown();
  }
  catch (std::runtime_error const&)
  {
  }
}

void TCPSocket::Close()
{
  auto socket = GetSocket();
  if (!socket)
    return;

  try
  {
    socket->close();
    ResetSocket();
  }
  catch (std::runtime_error const&)
  {
  }
}

int64_t TCPSocket::Read(void* data, size_t len, uint64_t iTimeoutMs /*= 0*/)
{
  auto socket = GetSocket();
  if (!socket)
    return -1;

  try
  {
    bool bError = false;
    uint64_t iNow = 0;
    uint64_t iTarget = 0;
    int64_t iBytesRead = 0;

    if (iTimeoutMs > 0)
    {
      iNow = MillisecondsSinceEpoch();
      iTarget = iNow + iTimeoutMs;
    }

    while (!bError && iBytesRead >= 0 && iBytesRead < static_cast<int64_t>(len) &&
           (iTimeoutMs == 0 || iTarget > iNow))
    {
      if (iTimeoutMs > 0)
      {
        const kissnet::socket_status status = socket->select(kissnet::fds_read, iTimeoutMs);

        if (status.value == kissnet::socket_status::timed_out ||
            status.value == kissnet::socket_status::errored)
        {
          bError = true;
        }
      }

      const auto [iReadResult, status] =
          (iTimeoutMs > 0) ? socket->recv(static_cast<std::byte*>(data) + iBytesRead,
                                          len - iBytesRead, false /* no wait */)
                           : socket->recv(static_cast<std::byte*>(data), len, true /* wait */);

      if (iTimeoutMs > 0)
        iNow = MillisecondsSinceEpoch();

      if (iReadResult < 0)
      {
        if (iTimeoutMs > 0 &&
            status.value == kissnet::socket_status::non_blocking_would_have_blocked)
          continue; // again

        return (iBytesRead > 0) ? iBytesRead : -1;
      }
      else if (iReadResult == 0 || (iReadResult != static_cast<int64_t>(len) && iTimeoutMs == 0))
      {
        bError = true;
      }

      iBytesRead += iReadResult;
    }

    return iBytesRead;
  }
  catch (std::runtime_error const&)
  {
    return -1;
  }
}

int64_t TCPSocket::Write(void* data, size_t len)
{
  auto socket = GetSocket();
  if (!socket)
    return -1;

  try
  {
    const auto [data_size, status] = socket->send(static_cast<std::byte*>(data), len);
    return data_size;
  }
  catch (std::runtime_error const&)
  {
    return -1;
  }
}

std::shared_ptr<kissnet::tcp_socket> TCPSocket::GetSocket(bool bCreate /*= false*/)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (bCreate && !m_socket)
    m_socket.reset(new kissnet::tcp_socket(m_endpoint));

  return m_socket;
}

void TCPSocket::ResetSocket()
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  m_socket.reset();
}
