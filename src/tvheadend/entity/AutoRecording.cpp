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

#include "AutoRecording.h"

using namespace tvheadend::entity;

AutoRecording::AutoRecording(const std::string &id /*= ""*/) :
    RecordingBase(id),
    m_minDuration(0),
    m_maxDuration(0),
    m_approxTime(0),
    m_startExtra(0),
    m_stopExtra(0),
    m_dupDetect(0),
    m_fulltext(0)
{
}

bool AutoRecording::operator==(const AutoRecording &right)
{
  return RecordingBase::operator==(right)     &&
         m_minDuration == right.m_minDuration &&
         m_maxDuration == right.m_maxDuration &&
         m_approxTime  == right.m_approxTime  &&
         m_startExtra  == right.m_startExtra  &&
         m_stopExtra   == right.m_stopExtra   &&
         m_dupDetect   == right.m_dupDetect   &&
         m_fulltext    == right.m_fulltext;
}

bool AutoRecording::operator!=(const AutoRecording &right)
{
  return !(*this == right);
}

void AutoRecording::SetMinDuration(uint32_t minDuration)
{
  m_minDuration = minDuration;
}

void AutoRecording::SetMaxDuration(uint32_t maxDuration)
{
  m_maxDuration = maxDuration;
}

time_t AutoRecording::GetStart() const
{
  if (m_approxTime == int32_t(-1)) // "any time"
    return 0;

  return LocaltimeToUTC(m_approxTime);
}

void AutoRecording::SetStart(int32_t start)
{
  m_approxTime = start;
}

time_t AutoRecording::GetStop() const
{
  if ((m_minDuration == 0) && (m_maxDuration == 0)) // no durations set => "any stop time"
    return 0;
  else if (m_minDuration == 0)
    return GetStart() + m_maxDuration;
  else if (m_maxDuration == 0)
    return GetStart() + m_minDuration;
  else
    return GetStart() + m_minDuration + ((m_maxDuration - m_minDuration) / 2);
}

int64_t AutoRecording::GetMarginStart() const
{
  return m_startExtra;
}

void AutoRecording::SetMarginStart(int64_t startExtra)
{
  m_startExtra = startExtra;
}

int64_t AutoRecording::GetMarginEnd() const
{
  return m_stopExtra;
}

void AutoRecording::SetMarginEnd(int64_t stopExtra)
{
  m_stopExtra = stopExtra;
}

uint32_t AutoRecording::GetDupDetect() const
{
  return m_dupDetect;
}

void AutoRecording::SetDupDetect(uint32_t dupDetect)
{
  m_dupDetect = dupDetect;
}

bool AutoRecording::GetFulltext() const
{
  return m_fulltext > 0;
}

void AutoRecording::SetFulltext(uint32_t fulltext)
{
  m_fulltext = fulltext;
}