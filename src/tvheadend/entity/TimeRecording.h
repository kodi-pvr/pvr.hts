/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "SeriesRecordingBase.h"

#include <cstdint>
#include <ctime>
#include <map>
#include <utility>

namespace tvheadend::entity
{

class TimeRecording : public SeriesRecordingBase
{
public:
  TimeRecording(const std::string& id = "");

  bool operator==(const TimeRecording& right);
  bool operator!=(const TimeRecording& right);

  time_t GetStart() const;
  void SetStart(int32_t start);

  time_t GetStop() const;
  void SetStop(int32_t stop);

private:
  int32_t m_start{0}; // Start time in minutes from midnight (up to 24*60).
  int32_t m_stop{0}; // Stop time in minutes from midnight (up to 24*60).
};

typedef std::map<std::string, TimeRecording> TimeRecordingsMap;
typedef std::pair<std::string, TimeRecording> TimeRecordingMapEntry;

} // namespace tvheadend::entity
