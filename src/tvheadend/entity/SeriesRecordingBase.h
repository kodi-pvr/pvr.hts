/*
 *  Copyright (C) 2024 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "RecordingBase.h"

#include <cstdint>
#include <ctime>
#include <string>

namespace tvheadend::entity
{

class SeriesRecordingBase : public RecordingBase
{
protected:
  SeriesRecordingBase(const std::string& id = "");

  bool operator==(const SeriesRecordingBase& right)
  {
    return RecordingBase::operator==(right) && m_sid == right.m_sid &&
           m_daysOfWeek == right.m_daysOfWeek && m_name == right.m_name &&
           m_directory == right.m_directory && m_owner == right.m_owner &&
           m_creator == right.m_creator;
  }

  bool operator!=(const SeriesRecordingBase& right) { return !(*this == right); }

public:
  std::string GetStringId() const { return m_sid; }
  void SetStringId(const std::string& sid) { m_sid = sid; }

  int GetDaysOfWeek() const { return m_daysOfWeek; }
  void SetDaysOfWeek(uint32_t daysOfWeek) { m_daysOfWeek = daysOfWeek; }

  const std::string& GetName() const { return m_name; }
  void SetName(const std::string& name) { m_name = name; }

  const std::string& GetDirectory() const { return m_directory; }
  void SetDirectory(const std::string& directory) { m_directory = directory; }

  void SetOwner(const std::string& owner) { m_owner = owner; }

  void SetCreator(const std::string& creator) { m_creator = creator; }

protected:
  static time_t LocaltimeToUTC(int32_t lctime);

private:
  static unsigned int GetNextIntId();

  std::string m_sid; // ID (string!) of dvr[Time|Auto]recEntry.
  uint32_t m_daysOfWeek{
      0}; // Bitmask - Days of week (0x01 = Monday, 0x40 = Sunday, 0x7f = Whole Week, 0 = Not set).
  std::string m_name; // Name.
  std::string m_directory; // Directory for the recording files.
  std::string m_owner; // Owner.
  std::string m_creator; // Creator.
};

} // namespace tvheadend::entity
