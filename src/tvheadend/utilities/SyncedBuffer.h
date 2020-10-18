/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>

namespace tvheadend
{
namespace utilities
{

template<typename T>
class SyncedBuffer
{
public:
  SyncedBuffer() = default;
  SyncedBuffer(size_t iSize) : m_maxSize(iSize) {}

  virtual ~SyncedBuffer()
  {
    while (!m_buffer.empty())
      m_buffer.pop();
    m_hasData = false;
    m_condition.notify_all();
  }

  size_t Size()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_buffer.size();
  }

  bool Push(T entry)
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_buffer.size() == m_maxSize)
      return false;

    m_buffer.push(entry);
    m_hasData = true;
    m_condition.notify_one();
    return true;
  }

  bool Pop(T& entry, int32_t iTimeoutMs = 0)
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_buffer.empty())
    {
      if (iTimeoutMs == 0)
        return false;

      if (!m_condition.wait_for(lock, std::chrono::milliseconds(iTimeoutMs),
                                [this] { return m_hasData == true; }))
        return false;
    }

    entry = m_buffer.front();
    m_buffer.pop();
    m_hasData = !m_buffer.empty();
    return true;
  }

private:
  size_t m_maxSize = 100;
  std::queue<T> m_buffer;
  std::mutex m_mutex;
  bool m_hasData = false;
  std::condition_variable m_condition;
};

} // namespace utilities
} // namespace tvheadend
