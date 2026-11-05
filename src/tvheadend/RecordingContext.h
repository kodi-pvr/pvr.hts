/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */
#pragma once

#include "kodi/AddonBase.h"
#include "kodi/addon-instance/pvr/Stream.h"

#include <charconv>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace tvheadend
{
class InstanceSettings;
inline constexpr char RECORDING_STREAMING_CAPABILITY[] = "dvr_streaming";

inline bool ParseRecordingId(const std::string& text, uint32_t& id)
{
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), id);
  return parsed.ec == std::errc() && parsed.ptr == text.data() + text.size();
}

// The generated instance ID binds InputStream creation to its PVR instance.
struct ATTR_DLL_LOCAL RecordingContext
{
  std::mutex mutex;
  bool alive = true;
  std::function<kodi::addon::PVRCodec(const std::string&)> codecLookup;
  std::shared_ptr<InstanceSettings> settings;
  std::string id;
};
} // namespace tvheadend
