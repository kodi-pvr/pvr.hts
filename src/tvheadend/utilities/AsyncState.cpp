/*
 *      Copyright (C) 2005-2014 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "AsyncState.h"

using namespace tvheadend::utilities;
using namespace P8PLATFORM;

struct Param {
  eAsyncState state;
  AsyncState *self;
};

AsyncState::AsyncState(int timeout)
{
  m_state   = ASYNC_NONE;
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

bool AsyncState::PredicateCallback ( void *p )
{
  Param *param = (Param*)p;
  return param->self->m_state >= param->state;
}

bool AsyncState::WaitForState(eAsyncState state)
{
  Param p;
  p.state = state;
  p.self  = this;

  CLockObject lock(m_mutex);
  return m_condition.Wait(m_mutex, AsyncState::PredicateCallback, (void*)&p, m_timeout);
}
