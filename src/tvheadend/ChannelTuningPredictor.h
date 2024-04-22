/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "entity/Channel.h"

#include <set>
#include <utility>

namespace tvheadend
{
namespace predictivetune
{

/**
 * Used to indicate that predictive tuning failed to determine which
 * channel to be tuned next
 */
const uint32_t CHANNEL_ID_NONE = -1;

/**
 * Defines a channel number
 */
class ChannelNumber
{
public:
  ChannelNumber() : m_channelNumber(0), m_subchannelNumber(0) {}

  ChannelNumber(uint32_t channelNumber, uint32_t subchannelNumber)
    : m_channelNumber(channelNumber), m_subchannelNumber(subchannelNumber)
  {
  }

  bool operator==(const ChannelNumber& right) const
  {
    return (m_channelNumber == right.m_channelNumber &&
            m_subchannelNumber == right.m_subchannelNumber);
  }

  bool operator<(const ChannelNumber& right) const
  {
    if (m_channelNumber == right.m_channelNumber)
      return m_subchannelNumber < right.m_subchannelNumber;

    return m_channelNumber < right.m_channelNumber;
  }

private:
  uint32_t m_channelNumber;
  uint32_t m_subchannelNumber;
};

/**
 * Defines a single channel ID/number pair
 */
typedef std::pair<uint32_t, ChannelNumber> ChannelPair;
typedef std::set<ChannelPair>::const_iterator ChannelPairIterator;

/**
 * Sorter for channel pairs
 */
struct SortChannelPair
{
  bool operator()(const ChannelPair& left, const ChannelPair& right) const
  {
    if (left.second < right.second)
      return true;
    
    if (right.second < left.second)
      return false;

    // if channel numbers are equal, consider channel id (which is unique)
    return left.first < right.first;
  }
};
} // namespace predictivetune

/**
 * This class holds a sorted set of channel "pairs" (ID -> number mapping) and
 * can be used to predict which channel ID should be tuned after a normal zap
 * is performed.
 */
class ChannelTuningPredictor
{
public:
  /**
   * Adds the specified channel
   * @param channel the channel
   */
  void AddChannel(const entity::Channel& channel);

  /**
   * Swaps the old channel with the new channel
   * @param oldChannel the channel that should be updated
   * @param newChannel the new channel
   */
  void UpdateChannel(const entity::Channel& oldChannel, const entity::Channel& newChannel);

  /**
   * Removes the channel with the specified channel ID
   * @param channelId the channel ID
   */
  void RemoveChannel(uint32_t channelId);

  /**
   * Attempts to predict the channel ID that should be tuned
   * @param tuningFrom the channel number we are tuning away from
   * @param tuningTo the channel number we are tuning to
   * @return the predicted channel number that should be tuned in advance, or
   *         CHANNEL_ID_NONE if nothing could be predicted
   */
  uint32_t PredictNextChannelId(uint32_t tuningFrom, uint32_t tuningTo) const;

private:
  /**
   * Constructs a channel pair from the specified channel number
   * @param channel the channel to construct a pair from
   */
  static predictivetune::ChannelPair MakeChannelPair(const entity::Channel& channel);

  /**
   * Returns an iterator positioned at the channel pair that matches the
   * specified channel ID. If no match is found, an end iterator is returned
   * @param channelId the channel ID
   * @return the iterator
   */
  predictivetune::ChannelPairIterator GetIterator(uint32_t channelId) const;

  /**
   * Set of pairs which map channel IDs to channel numbers. A custsom comparator is
   * used to ensure that all inserted pairs are always sorted by the channel number.
   * This way we can get the next/previous channel by simply adjusting iterators.
   */
  std::set<predictivetune::ChannelPair, predictivetune::SortChannelPair> m_channels;
};

} // namespace tvheadend
