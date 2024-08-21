/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <condition_variable>
#include <mutex>

namespace tvheadend
{
namespace utilities
{

/**
 * Represents the possible states
 */
enum eAsyncState
{
  ASYNC_NONE = 0,
  ASYNC_INIT = 1,
  ASYNC_CHN = 2,
  ASYNC_DVR = 3,
  ASYNC_EPG = 4,
  ASYNC_DONE = 5
};

/**
 * State tracker for the initial sync process. This class is thread-safe.
 */
class AsyncState
{
public:
  AsyncState(int timeout);

  virtual ~AsyncState() {}

  /**
   * @return the current state
   */
  eAsyncState GetState();

  /**
   * Changes the current state to "state"
   * @param state the new state
   */
  void SetState(eAsyncState state);

  /**
   * Waits for the current state to change into "state" or higher
   * before the timeout is reached
   * @param state the minimum state desired
   * @return whether the state changed or not
   */
  bool WaitForState(eAsyncState state);

private:
  eAsyncState m_state;
  std::recursive_mutex m_mutex;
  std::condition_variable_any m_condition;
  int m_timeout;
};

} // namespace utilities
} // namespace tvheadend
