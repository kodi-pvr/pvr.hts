/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

namespace tvheadend
{
namespace status
{

/**
 * Represents the current demuxer packet queue
 */
struct QueueStatus
{
  /**
   * Number of data packets in queue
   */
  uint32_t packets;

  /**
   * Number of bytes in queue.
   */
  uint32_t bytes;

  /**
   * Estimated delay of queue (in µs)
   */
  uint32_t delay;

  /**
   * Number of B-frames dropped
   */
  uint32_t bdrops;

  /**
   * Number of P-frames dropped
   */
  uint32_t pdrops;

  /**
   * Number of I-frames dropped
   */
  uint32_t idrops;

  /**
   * Constructor
   */
  QueueStatus() : packets(0), bytes(0), delay(0), bdrops(0), pdrops(0), idrops(0) {}
};

} // namespace status
} // namespace tvheadend
