/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace tvheadend
{
namespace utilities
{

class RDSExtractor
{
public:
  RDSExtractor() = default;
  virtual ~RDSExtractor() = default;

  virtual uint8_t Decode(const uint8_t* data, size_t len) = 0;

  uint8_t GetRDSDataLength() const { return m_rdsLen; }
  uint8_t* GetRDSData() const { return m_rdsData; }

  void Reset()
  {
    m_rdsLen = 0;
    delete[] m_rdsData;
    m_rdsData = nullptr;
  }

protected:
  uint8_t m_rdsLen = 0;
  uint8_t* m_rdsData = nullptr;
};

class RDSExtractorMP2 : public RDSExtractor
{
public:
  uint8_t Decode(const uint8_t* data, size_t len) override;
};

class RDSExtractorAAC : public RDSExtractor
{
public:
  uint8_t Decode(const uint8_t* data, size_t len) override;
};

} // namespace utilities
} // namespace tvheadend
