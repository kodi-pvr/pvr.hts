/*
 *      Copyright (C) 2005-2015 Team Kodi
 *      http://kodi.tv
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

#include <time.h>
#include "Tvheadend.h"

#include "HTSPTypes.h"

namespace htsp
{

Tag::Tag(uint32_t id /*= 0*/) :
  m_dirty(false),
  m_id   (id),
  m_index(0)
{
}

bool Tag::operator==(const Tag &right)
{
  return m_id       == right.m_id &&
         m_index    == right.m_index &&
         m_name     == right.m_name &&
         m_icon     == right.m_icon &&
         m_channels == right.m_channels;
}

bool Tag::operator!=(const Tag &right)
{
  return !(*this == right);
}

bool Tag::IsDirty() const
{
  return m_dirty;
}

void Tag::SetDirty(bool bDirty)
{
  m_dirty = bDirty;
}

uint32_t Tag::GetId() const
{
   return m_id;
}

uint32_t Tag::GetIndex() const
{
  return m_index;
}

void Tag::SetIndex(uint32_t index)
{
  m_index = index;
}

const std::string& Tag::GetName() const
{
  return m_name;
}

void Tag::SetName(const std::string& name)
{
  m_name = name;
}

void Tag::SetIcon(const std::string& icon)
{
  m_icon = icon;
}

const std::vector<uint32_t>& Tag::GetChannels() const
{
  return m_channels;
}

std::vector<uint32_t>& Tag::GetChannels()
{
  return m_channels;
}

bool Tag::ContainsChannelType(bool bRadio) const
{
  std::vector<uint32_t>::const_iterator it;
  SChannels::const_iterator cit;
  const SChannels& channels = tvh->GetChannels();

  for (it = m_channels.begin(); it != m_channels.end(); ++it)
  {
    if ((cit = channels.find(*it)) != channels.end())
    {
      if (bRadio == cit->second.radio)
        return true;
    }
  }
  return false;
}

/* **************************************************************************
 * RecordingBase
 * *************************************************************************/

RecordingBase::RecordingBase(const std::string &id /*= ""*/) :
  m_iId(GetNextIntId()),
  m_dirty(false),
  m_id(id),
  m_enabled(0),
  m_daysOfWeek(0),
  m_retention(0),
  m_priority(0),
  m_channel(0)
{
}

bool RecordingBase::operator==(const RecordingBase &right)
{
  return m_iId         == right.m_iId         &&
         m_id          == right.m_id          &&
         m_enabled     == right.m_enabled     &&
         m_daysOfWeek  == right.m_daysOfWeek  &&
         m_retention   == right.m_retention   &&
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

unsigned int RecordingBase::GetIntId() const
{
  return m_iId;
}

bool RecordingBase::IsDirty() const
{
  return m_dirty;
}

void RecordingBase::SetDirty(bool bDirty)
{
  m_dirty = bDirty;
}

std::string RecordingBase::GetStringId() const
{
  return m_id;
}

void RecordingBase::SetStringId(const std::string &id)
{
  m_id = id;
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

uint32_t RecordingBase::GetRetention() const
{
  return m_retention;
}

void RecordingBase::SetRetention(uint32_t retention)
{
  m_retention = retention;
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
  tm_time->tm_isdst = 0;

  t = mktime(tm_time);

  /* convert to UTC. */
  return mktime(gmtime(&t));
}

// static
int RecordingBase::GetNextIntId()
{
  static unsigned int intId = 0;
  return ++intId;
}

/* **************************************************************************
 * TimeRecording
 * *************************************************************************/

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

/* **************************************************************************
 * AutoRecording
 * *************************************************************************/

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

} // namespace htsp
