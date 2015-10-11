#pragma once

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

#include "RecordingBase.h"
#include <map>

namespace tvheadend
{
  namespace entity
  {
    class AutoRecording : public RecordingBase
    {
    public:
      AutoRecording(const std::string &id = "");

      bool operator==(const AutoRecording &right);
      bool operator!=(const AutoRecording &right);

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

    private:
      int32_t  m_startWindowBegin; // Begin of the starting window (minutes from midnight).
      int32_t  m_startWindowEnd;   // End of the starting window (minutes from midnight).
      int64_t  m_startExtra;  // Extra start minutes (pre-time).
      int64_t  m_stopExtra;   // Extra stop minutes (post-time).
      uint32_t m_dupDetect;   // duplicate episode detect (record: 0 = all, 1 = different episode number,
                              //                                   2 = different subtitle, 3 = different description,
                              //                                   4 = once per week, 5 = once per day).
      uint32_t m_fulltext;    // Fulltext epg search.
    };

    typedef std::map<std::string, AutoRecording> AutoRecordingsMap;
    typedef std::pair<std::string, AutoRecording> AutoRecordingMapEntry;
  }
}
