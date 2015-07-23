/*
 *      Copyright (C) 2005-2011 Team XBMC
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

#include "TimeRecording.h"

using namespace tvheadend::entity;

TimeRecording::TimeRecording(const std::string &id /*= ""*/) :
    RecordingBase(id),
    m_start(0),
    m_stop(0)
{
}

bool TimeRecording::operator==(const TimeRecording &right)
{
  return RecordingBase::operator==(right) &&
         m_start       == right.m_start   &&
         m_stop        == right.m_stop;
}

bool TimeRecording::operator!=(const TimeRecording &right)
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