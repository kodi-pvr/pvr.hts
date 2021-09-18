/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "AutoRecording.h"

#include "../Settings.h"

using namespace tvheadend;
using namespace tvheadend::entity;

AutoRecording::AutoRecording(const std::string& id /*= ""*/)
  : RecordingBase(id),
    m_startWindowBegin(0),
    m_startWindowEnd(0),
    m_startExtra(0),
    m_stopExtra(0),
    m_dupDetect(0),
    m_fulltext(0)
{
}

bool AutoRecording::operator==(const AutoRecording& right)
{
  return RecordingBase::operator==(right) && m_startWindowBegin == right.m_startWindowBegin &&
         m_startWindowEnd == right.m_startWindowEnd && m_startExtra == right.m_startExtra &&
         m_stopExtra == right.m_stopExtra && m_dupDetect == right.m_dupDetect &&
         m_fulltext == right.m_fulltext && m_seriesLink == right.m_seriesLink;
}

bool AutoRecording::operator!=(const AutoRecording& right)
{
  return !(*this == right);
}

time_t AutoRecording::GetStart() const
{
  if (Settings::GetInstance().GetAutorecApproxTime())
  {
    /* Calculate the approximate start time from the starting window */
    if ((m_startWindowBegin == -1) ||
        (m_startWindowEnd == -1)) // no starting window set => "any time"
    {
      return 0;
    }
    else if (m_startWindowEnd < m_startWindowBegin)
    {
      /* End of start window is a day in the future */
      int32_t newEnd = m_startWindowEnd + (24 * 60);
      int32_t newStart = m_startWindowBegin + (newEnd - m_startWindowBegin) / 2;

      if (newStart > (24 * 60))
        newStart -= (24 * 60);

      return LocaltimeToUTC(newStart);
    }
    else
      return LocaltimeToUTC(m_startWindowBegin + (m_startWindowEnd - m_startWindowBegin) / 2);
  }
  else
  {
    if (m_startWindowBegin == -1) // "any time"
      return 0;

    return LocaltimeToUTC(m_startWindowBegin);
  }
}

void AutoRecording::SetStartWindowBegin(int32_t start)
{
  m_startWindowBegin = start;
}

time_t AutoRecording::GetStop() const
{
  if (Settings::GetInstance().GetAutorecApproxTime())
  {
    /* Tvh doesn't have an approximate stop time => "any time" */
    return 0;
  }
  else
  {
    if (m_startWindowEnd == -1) // "any time"
      return 0;

    return LocaltimeToUTC(m_startWindowEnd);
  }
}

void AutoRecording::SetStartWindowEnd(int32_t end)
{
  m_startWindowEnd = end;
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

const std::string& AutoRecording::GetSeriesLink() const
{
  return m_seriesLink;
}

void AutoRecording::SetSeriesLink(const std::string& seriesLink)
{
  m_seriesLink = seriesLink;
}
