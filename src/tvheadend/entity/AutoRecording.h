/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "RecordingBase.h"

#include <map>

namespace tvheadend
{
namespace entity
{

class AutoRecording : public RecordingBase
{
public:
  AutoRecording(const std::string& id = "");

  bool operator==(const AutoRecording& right);
  bool operator!=(const AutoRecording& right);

  time_t GetStart() const;
  void SetStartWindowBegin(int32_t begin);

  time_t GetStop() const;
  void SetStartWindowEnd(int32_t end);

  int64_t GetMarginStart() const;
  void SetMarginStart(int64_t startExtra);

  int64_t GetMarginEnd() const;
  void SetMarginEnd(int64_t stopExtra);

  uint32_t GetDupDetect() const;
  void SetDupDetect(uint32_t dupDetect);

  bool GetFulltext() const;
  void SetFulltext(uint32_t fulltext);

  const std::string& GetSeriesLink() const;
  void SetSeriesLink(const std::string& seriesLink);

private:
  int32_t m_startWindowBegin; // Begin of the starting window (minutes from midnight).
  int32_t m_startWindowEnd; // End of the starting window (minutes from midnight).
  int64_t m_startExtra; // Extra start minutes (pre-time).
  int64_t m_stopExtra; // Extra stop minutes (post-time).
  uint32_t m_dupDetect; // duplicate episode detect (numeric values: see dvr_autorec_dedup_t).
  uint32_t m_fulltext; // Fulltext epg search.
  std::string m_seriesLink; // Series link.
};

typedef std::map<std::string, AutoRecording> AutoRecordingsMap;
typedef std::pair<std::string, AutoRecording> AutoRecordingMapEntry;

} // namespace entity
} // namespace tvheadend
