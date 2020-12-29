/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
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
    if (!m_socket)
      m_socket.reset(new kissnet::tcp_socket(m_endpoint));

    int status = kissnet::socket_status::valid;

    m_socket->set_non_blocking(true);
    status = m_socket->connect().value;
    m_socket->set_non_blocking(false);

    if (status == kissnet::socket_status::non_blocking_would_have_blocked)
    {
      status = m_socket->select(kissnet::fds_write | kissnet::fds_except, iTimeoutMs);

      if (status == kissnet::socket_status::valid)
        status = m_socket->get_status().value; // re-check
    }

    if (status == kissnet::socket_status::valid)
      m_socket->set_tcp_no_delay(true);

    return status == kissnet::socket_status::valid;
  }
  catch (std::runtime_error const&)
  {
    m_socket.reset();
    return false;
  }
}

void TCPSocket::Shutdown()
{
  if (!m_socket)
    return;

  try
  {
    m_socket->shutdown();
  }
  catch (std::runtime_error const&)
  {
  }
}

void TCPSocket::Close()
{
  if (!m_socket)
    return;

  try
  {
    m_socket->close();
    m_socket.reset();
  }
  catch (std::runtime_error const&)
  {
  }
}

int64_t TCPSocket::Read(void* data, size_t len, uint64_t iTimeoutMs /*= 0*/)
{
  if (!m_socket)
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
        const kissnet::socket_status status = m_socket->select(kissnet::fds_read, iTimeoutMs);

        if (status.value == kissnet::socket_status::timed_out ||
            status.value == kissnet::socket_status::errored)
        {
          bError = true;
        }
      }

      const auto [iReadResult, status] =
          (iTimeoutMs > 0) ? m_socket->recv(static_cast<std::byte*>(data) + iBytesRead,
                                            len - iBytesRead, false /* no wait */)
                           : m_socket->recv(static_cast<std::byte*>(data), len, true /* wait */);

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
  if (!m_socket)
    return -1;

  try
  {
    const auto [data_size, status] = m_socket->send(static_cast<std::byte*>(data), len);
    return data_size;
  }
  catch (std::runtime_error const&)
  {
    return -1;
  }
}
