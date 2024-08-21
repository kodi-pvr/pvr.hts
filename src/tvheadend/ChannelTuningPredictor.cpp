/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "ChannelTuningPredictor.h"

#include <algorithm>

using namespace tvheadend;
using namespace tvheadend::entity;
using namespace tvheadend::predictivetune;

void ChannelTuningPredictor::AddChannel(const Channel& channel)
{
  m_channels.insert(MakeChannelPair(channel));
}

void ChannelTuningPredictor::UpdateChannel(const Channel& oldChannel, const Channel& newChannel)
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

ChannelPair ChannelTuningPredictor::MakeChannelPair(const entity::Channel& channel)
{
  return ChannelPair(channel.GetId(), ChannelNumber(channel.GetNum(), channel.GetNumMinor()));
}

ChannelPairIterator ChannelTuningPredictor::GetIterator(uint32_t channelId) const
{
  return std::find_if(m_channels.cbegin(), m_channels.cend(),
                      [channelId](const ChannelPair& channel)
                      { return channel.first == channelId; });
}

uint32_t ChannelTuningPredictor::PredictNextChannelId(uint32_t tuningFrom, uint32_t tuningTo) const
{
  auto fromIt = GetIterator(tuningFrom);
  auto toIt = GetIterator(tuningTo);

  /* Determine the respective channel numbers as well as the first channel */
  const ChannelNumber& firstNum = m_channels.cbegin()->second;

  /* Create an iterator for the predicted channel. If prediction succeeds,
   * it will point at the channel we should tune to */
  std::set<ChannelPair>::iterator predictedIt = m_channels.cend();

  if (fromIt == m_channels.cend() || std::next(fromIt, 1) == toIt || toIt->second == firstNum)
  {
    /* Tuning up or if we're tuning the first channel */
    predictedIt = ++toIt;
  }
  else if (std::prev(fromIt, 1) == toIt)
  {
    /* Tuning down */
    predictedIt = --toIt;
  }

  if (predictedIt != m_channels.cend())
    return predictedIt->first;
  else
    return CHANNEL_ID_NONE;
}
