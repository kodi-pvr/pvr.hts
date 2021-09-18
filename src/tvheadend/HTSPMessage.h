/*
 *  Copyright (C) 2017-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

extern "C"
{
#include "libhts/htsmsg.h"
}

#include <string>

namespace tvheadend
{

/*
 * HTSP Message
 */
class HTSPMessage
{
public:
  HTSPMessage(const std::string& method = "", htsmsg_t* msg = nullptr)
    : m_method(method), m_msg(msg)
  {
  }

  HTSPMessage(const HTSPMessage& msg) : m_method(msg.m_method), m_msg(msg.m_msg)
  {
    msg.m_msg = nullptr;
  }

  ~HTSPMessage()
  {
    if (m_msg)
      htsmsg_destroy(m_msg);
  }

  HTSPMessage& operator=(const HTSPMessage& msg)
  {
    if (this != &msg)
    {
      if (m_msg)
        htsmsg_destroy(m_msg);

      m_method = msg.m_method;
      m_msg = msg.m_msg;
      msg.m_msg = nullptr; // ownership is passed
    }
    return *this;
  }

  const std::string& GetMethod() const { return m_method; }
  htsmsg_t* GetHTSPMessage() const { return m_msg; }

  void ClearMessage()
  {
    htsmsg_destroy(m_msg);
    m_msg = nullptr;
  }

private:
  std::string m_method;
  mutable htsmsg_t* m_msg;
};

} // namespace tvheadend
