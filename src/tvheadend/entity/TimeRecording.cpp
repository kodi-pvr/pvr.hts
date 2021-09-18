/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "TimeRecording.h"

using namespace tvheadend::entity;

TimeRecording::TimeRecording(const std::string& id /*= ""*/)
  : RecordingBase(id), m_start(0), m_stop(0)
{
}

bool TimeRecording::operator==(const TimeRecording& right)
{
  return RecordingBase::operator==(right) && m_start == right.m_start && m_stop == right.m_stop;
}

bool TimeRecording::operator!=(const TimeRecording& right)
{
  return !(*this == right);
}

time_t TimeRecording::GetStart() const
{
  if (m_start == int32_t(-1)) // "any time"
    return 0;

  return LocaltimeToUTC(m_start);
}

void TimeRecording::SetStart(int32_t start)
{
  m_start = start;
}

time_t TimeRecording::GetStop() const
{
  if (m_stop == int32_t(-1)) // "any time"
    return 0;

  return LocaltimeToUTC(m_stop);
}

void TimeRecording::SetStop(int32_t stop)
{
  m_stop = stop;
}
