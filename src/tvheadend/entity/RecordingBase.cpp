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

#include <algorithm>

#include "RecordingBase.h"

using namespace tvheadend::entity;

RecordingBase::RecordingBase(const std::string &id /*= ""*/) :
    m_sid(id),
    m_enabled(0),
    m_daysOfWeek(0),
    m_lifetime(0),
    m_priority(0),
    m_channel(0)
{
  m_id = GetNextIntId();
}

bool RecordingBase::operator==(const RecordingBase &right)
{
  return m_id          == right.m_id          &&
         m_enabled     == right.m_enabled     &&
         m_daysOfWeek  == right.m_daysOfWeek  &&
         m_lifetime    == right.m_lifetime    &&
         m_priority    == right.m_priority    &&
         m_title       == right.m_title       &&
         m_name        == right.m_name        &&
         m_directory   == right.m_directory   &&
         m_owner       == right.m_owner       &&
         m_creator     == right.m_creator     &&
         m_channel     == right.m_channel;
}

bool RecordingBase::operator!=(const RecordingBase &right)
{
  return !(*this == right);
}

std::string RecordingBase::GetStringId() const
{
  return m_sid;
}

void RecordingBase::SetStringId(const std::string &id)
{
  m_sid = id;
}

bool RecordingBase::IsEnabled() const
{
  return m_enabled != 0;
}

void RecordingBase::SetEnabled(uint32_t enabled)
{
  m_enabled = enabled;
}

int RecordingBase::GetDaysOfWeek() const
{
  return m_daysOfWeek;
}

void RecordingBase::SetDaysOfWeek(uint32_t daysOfWeek)
{
  m_daysOfWeek = daysOfWeek;
}

uint32_t RecordingBase::GetLifetime() const
{
  return m_lifetime;
}

void RecordingBase::SetLifetime(uint32_t lifetime)
{
  m_lifetime = lifetime;
}

uint32_t RecordingBase::GetPriority() const
{
  return m_priority;
}

void RecordingBase::SetPriority(uint32_t priority)
{
  m_priority = priority;
}

const std::string& RecordingBase::GetTitle() const
{
  return m_title;
}

void RecordingBase::SetTitle(const std::string &title)
{
  m_title = title;
}

const std::string& RecordingBase::GetName() const
{
  return m_name;
}

void RecordingBase::SetName(const std::string &name)
{
  m_name = name;
}

const std::string& RecordingBase::GetDirectory() const
{
  return m_directory;
}

void RecordingBase::SetDirectory(const std::string &directory)
{
  m_directory = directory;
}

void RecordingBase::SetOwner(const std::string &owner)
{
  m_owner = owner;
}

void RecordingBase::SetCreator(const std::string &creator)
{
  m_creator = creator;
}

uint32_t RecordingBase::GetChannel() const
{
  return m_channel;
}

void RecordingBase::SetChannel(uint32_t channel)
{
  m_channel = channel;
}

// static
time_t RecordingBase::LocaltimeToUTC(int32_t lctime)
{
  /* Note: lctime contains minutes from midnight (up to 24*60) as local time. */

  /* complete lctime with current year, month, day, ... */
  time_t t = time(NULL);
  struct tm *tm_time = localtime(&t);

  tm_time->tm_hour  = lctime / 60;
  tm_time->tm_min   = lctime % 60;
  tm_time->tm_sec   = 0;

  return mktime(tm_time);
}

// static
unsigned int RecordingBase::GetNextIntId()
{
  static unsigned int intId = 0;
  return ++intId;
}
