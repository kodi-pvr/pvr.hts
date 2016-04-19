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

#include "ChannelTuningPredictor.h"
#include <algorithm>

using namespace tvheadend;
using namespace tvheadend::entity;
using namespace tvheadend::predictivetune;

void ChannelTuningPredictor::AddChannel(const Channel &channel)
{
  m_channels.insert(MakeChannelPair(channel));
}

void ChannelTuningPredictor::UpdateChannel(const Channel &oldChannel, const Channel &newChannel)
{
  m_channels.erase(MakeChannelPair(oldChannel));
  m_channels.insert(MakeChannelPair(newChannel));
}

void ChannelTuningPredictor::RemoveChannel(uint32_t channelId)
{
  auto it = GetIterator(channelId);

  if (it != m_channels.end())
    m_channels.erase(it);
}

ChannelPair ChannelTuningPredictor::MakeChannelPair(const entity::Channel &channel)
{
  return ChannelPair(channel.GetId(), channel.GetNum());
}

ChannelPairIterator ChannelTuningPredictor::GetIterator(uint32_t channelId) const
{
  return std::find_if(
      m_channels.cbegin(),
      m_channels.cend(),
      [channelId](const ChannelPair &channel)
      {
        return channel.first == channelId;
      }
  );
}

uint32_t ChannelTuningPredictor::PredictNextChannelId(uint32_t tuningFrom, uint32_t tuningTo) const
{
  auto fromIt = GetIterator(tuningFrom);
  auto toIt = GetIterator(tuningTo);

  /* Determine the respective channel numbers as well as the first channel */
  uint32_t firstNum = m_channels.cbegin()->second;

  /* Create an iterator for the predicted channel. If prediction succeeds,
   * it will point at the channel we should tune to */
  std::set<ChannelPair>::iterator predictedIt = m_channels.cend();

  if (fromIt == m_channels.cend() || std::next(fromIt, 1) == toIt || toIt->second == firstNum) {
    /* Tuning up or if we're tuning the first channel */
    predictedIt = ++toIt;
  }
  else if (std::prev(fromIt, 1) == toIt) {
    /* Tuning down */
    predictedIt = --toIt;
  }

  if (predictedIt != m_channels.cend())
    return predictedIt->first;
  else
    return CHANNEL_ID_NONE;
}
