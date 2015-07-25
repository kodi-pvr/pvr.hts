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
    class TimeRecording : public RecordingBase
    {
    public:
      TimeRecording(const std::string &id = "");

      bool operator==(const TimeRecording &right);
      bool operator!=(const TimeRecording &right);

      time_t GetStart() const;
      void SetStart(int32_t start);

      time_t GetStop() const;
      void SetStop(int32_t stop);

    private:
      int32_t m_start; // Start time in minutes from midnight (up to 24*60).
      int32_t m_stop;  // Stop time in minutes from midnight (up to 24*60).
    };

    typedef std::map<std::string, TimeRecording> TimeRecordingsMap;
    typedef std::pair<std::string, TimeRecording> TimeRecordingMapEntry;
  }
}
