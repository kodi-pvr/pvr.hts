#pragma once

/*
 *      Copyright (C) 2017 Team Kodi
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

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
  htsmsg_t* GetMessage() const { return m_msg; }

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
