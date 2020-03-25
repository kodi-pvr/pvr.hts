/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <string>

namespace tvheadend
{
namespace status
{

/**
 * Represents the current signal quality
 */
struct Quality
{
  /**
   * Frontend status
   */
  std::string fe_status;

  /**
   * Signal to noise ratio
   */
  uint32_t fe_snr;

  /**
   * Signal status percentage
   */
  uint32_t fe_signal;

  /**
   * Bit error rate
   */
  uint32_t fe_ber;

  /**
   * Uncorrected blocks
   */
  uint32_t fe_unc;

  /**
   * Constructor
   */
  Quality() : fe_snr(0), fe_signal(0), fe_ber(0), fe_unc(0) {}

  /**
   * Clears the current status
   */
  void Clear()
  {
    fe_status.clear();
    fe_snr = 0;
    fe_signal = 0;
    fe_ber = 0;
    fe_unc = 0;
  }
};

} // namespace status
} // namespace tvheadend
