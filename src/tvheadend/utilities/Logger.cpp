/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Logger.h"

#include "p8-platform/util/StringUtils.h"

using namespace tvheadend::utilities;

Logger::Logger()
{
  // Use an empty implementation by default
  SetImplementation([](LogLevel level, const char* message) {});
}

Logger& Logger::GetInstance()
{
  static Logger instance;
  return instance;
}

void Logger::Log(LogLevel level, const char* message, ...)
{
  auto& logger = GetInstance();

  va_list arguments;
  va_start(arguments, message);
  const std::string logMessage = StringUtils::FormatV(message, arguments);
  va_end(arguments);

  logger.m_implementation(level, logMessage.c_str());
}

void Logger::SetImplementation(LoggerImplementation implementation)
{
  m_implementation = implementation;
}
