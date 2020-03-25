/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>
#include <string>

namespace tvheadend
{
namespace status
{

/**
 * Contains information about the descrambler used
 */
class DescrambleInfo
{
public:
  DescrambleInfo();
  void Clear();

  int64_t GetPid() const;
  void SetPid(uint32_t pid);

  int64_t GetCaid() const;
  void SetCaid(uint32_t caid);

  int64_t GetProvid() const;
  void SetProvid(uint32_t provid);

  int64_t GetEcmTime() const;
  void SetEcmTime(uint32_t ecmTime);

  int64_t GetHops() const;
  void SetHops(uint32_t hops);

  std::string GetCardSystem() const;
  void SetCardSystem(const std::string& cardSystem);

  std::string GetReader() const;
  void SetReader(const std::string& reader);

  std::string GetFrom() const;
  void SetFrom(const std::string& from);

  std::string GetProtocol() const;
  void SetProtocol(const std::string& protocol);

private:
  int64_t m_pid;
  int64_t m_caid;
  int64_t m_provid;
  int64_t m_ecmTime;
  int64_t m_hops;
  std::string m_cardSystem;
  std::string m_reader;
  std::string m_from;
  std::string m_protocol;
};

} // namespace status
} // namespace tvheadend
