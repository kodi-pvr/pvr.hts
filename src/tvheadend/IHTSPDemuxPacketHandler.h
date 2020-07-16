/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "kodi/addon-instance/inputstream/DemuxPacket.h"
#include "kodi/addon-instance/pvr/Stream.h"

namespace tvheadend
{

/*
 * HTSP Demux Packet Handler interface
 */
class IHTSPDemuxPacketHandler
{
public:
  virtual ~IHTSPDemuxPacketHandler() = default;

  virtual kodi::addon::PVRCodec GetCodecByName(const std::string& codecName) const = 0;
  virtual DEMUX_PACKET* AllocateDemuxPacket(int iDataSize) = 0;
  virtual void FreeDemuxPacket(DEMUX_PACKET* pPacket) = 0;
};

} // namespace tvheadend
