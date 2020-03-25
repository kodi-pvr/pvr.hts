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
 * Represents the current timeshift status
 */
struct TimeshiftStatus
{

  /**
   * Whether the buffer is full or not
   */
  bool full;

  /**
   * Current position relative to live
   */
  int64_t shift;

  /**
   * PTS of the first frame in the buffer
   */
  int64_t start;

  /**
   * PTS of the last frame in the buffer
   */
  int64_t end;

  /**
   * Constructor
   */
  TimeshiftStatus() { Clear(); }

  /**
   * Clears the current status
   */
  void Clear()
  {
    full = false;
    shift = 0;
    start = 0;
    ;
    end = 0;
  }
};

} // namespace status
} // namespace tvheadend
