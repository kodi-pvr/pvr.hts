/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "p8-platform/threads/mutex.h"

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
  ASYNC_CHN = 1,
  ASYNC_DVR = 2,
  ASYNC_EPG = 3,
  ASYNC_DONE = 4
};

/**
 * State tracker for the initial sync process. This class is thread-safe.
 */
class AsyncState
{
public:
  AsyncState(int timeout);

  virtual ~AsyncState(){};

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
  static bool PredicateCallback(void* param);

  eAsyncState m_state;
  P8PLATFORM::CMutex m_mutex;
  P8PLATFORM::CCondition<bool> m_condition;
  int m_timeout;
};

} // namespace utilities
} // namespace tvheadend
