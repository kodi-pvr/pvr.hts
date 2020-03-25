/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "AsyncState.h"

using namespace tvheadend::utilities;
using namespace P8PLATFORM;

struct Param
{
  eAsyncState state;
  AsyncState* self;
};

AsyncState::AsyncState(int timeout)
{
  m_state = ASYNC_NONE;
  m_timeout = timeout;
}

eAsyncState AsyncState::GetState()
{
  P8PLATFORM::CLockObject lock(m_mutex);
  return m_state;
}

void AsyncState::SetState(eAsyncState state)
{
  CLockObject lock(m_mutex);
  m_state = state;
  m_condition.Broadcast();
}

bool AsyncState::PredicateCallback(void* p)
{
  Param* param = reinterpret_cast<Param*>(p);
  return param->self->m_state >= param->state;
}

bool AsyncState::WaitForState(eAsyncState state)
{
  Param p;
  p.state = state;
  p.self = this;

  CLockObject lock(m_mutex);
  return m_condition.Wait(m_mutex, AsyncState::PredicateCallback, &p, m_timeout);
}
