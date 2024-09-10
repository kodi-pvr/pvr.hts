/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "RecordingBase.h"

#include "../utilities/LifetimeMapper.h"

using namespace tvheadend::entity;

int RecordingBase::GetLifetime() const
{
  return utilities::LifetimeMapper::TvhToKodi(m_lifetime);
}
