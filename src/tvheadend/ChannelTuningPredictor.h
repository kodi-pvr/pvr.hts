#pragma once

/*
 *      Copyright (C) 2005-2015 Team Kodi
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
     * Defines a single channel ID/number pair
     */
    typedef std::pair<uint32_t, uint32_t> ChannelPair;
    typedef std::set<predictivetune::ChannelPair>::const_iterator ChannelPairIterator;

    /**
     * Sorter for channel pairs
     */
    struct SortChannelPair
    {
      bool operator()(const ChannelPair &left, const ChannelPair &right) const
      {
        return left.second < right.second;
      }
    };
  }

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
    void AddChannel(const entity::Channel &channel);

    /**
     * Swaps the old channel with the new channel
     * @param oldChannel the channel that should be updated
     * @param newChannel the new channel
     */
    void UpdateChannel(const entity::Channel &oldChannel, const entity::Channel &newChannel);

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
    static predictivetune::ChannelPair MakeChannelPair(const entity::Channel &channel);

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
}
