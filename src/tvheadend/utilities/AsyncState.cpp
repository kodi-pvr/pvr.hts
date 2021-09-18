/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "AsyncState.h"

#include <chrono>

using namespace tvheadend::utilities;

AsyncState::AsyncState(int timeout)
{
  m_state = ASYNC_NONE;
  m_timeout = timeout;
}

eAsyncState AsyncState::GetState()
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  return m_state;
}

void AsyncState::SetState(eAsyncState state)
{
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  m_state = state;
  m_condition.notify_all();
}

bool AsyncState::WaitForState(eAsyncState state)
{
  std::unique_lock<std::recursive_mutex> lock(m_mutex);
  return m_condition.wait_for(lock, std::chrono::milliseconds(m_timeout),
                              [this, state] { return m_state >= state; });
}
