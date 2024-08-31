/*
 *  Copyright (C) 2024 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "SeriesRecordingBase.h"

#include <ctime>

using namespace tvheadend::entity;

SeriesRecordingBase::SeriesRecordingBase(const std::string& id /*= ""*/) : m_sid(id)
{
  m_id = GetNextIntId();
}

// static
time_t SeriesRecordingBase::LocaltimeToUTC(int32_t lctime)
{
  /* Note: lctime contains minutes from midnight (up to 24*60) as local time. */

  /* complete lctime with current year, month, day, ... */
  time_t t = std::time(nullptr);
  struct tm* tm_time = std::localtime(&t);

  tm_time->tm_hour = lctime / 60;
  tm_time->tm_min = lctime % 60;
  tm_time->tm_sec = 0;

  return std::mktime(tm_time);
}

// static
unsigned int SeriesRecordingBase::GetNextIntId()
{
  static unsigned int intId = 0;
  return ++intId;
}
