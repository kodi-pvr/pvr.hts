#pragma once

/*
 *      Copyright (C) 2005-2014 Team XBMC
 *      http://www.xbmc.org
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

#include <map>
#include "platform/threads/mutex.h"

extern "C" {
#include <sys/types.h>
#include "libhts/htsmsg.h"
}

namespace tvheadend
{
  namespace htsp
  {

    class Response;
    typedef std::map<uint32_t, Response*> ResponseList;

    /**
     * Wrapper for HTSP responses. When a request is sent, a place holder
     * in a ResponseList is created. When the response is received, the
     * actual HTSP response is stored in the object using Set(). Get() is
     * then used to retrieve the response.
     */
    class Response
    {
    public:
      Response();
      ~Response();

      htsmsg_t *Get(PLATFORM::CMutex &mutex, uint32_t timeout);
      void Set(htsmsg_t *m);

    private:
      PLATFORM::CCondition<volatile bool> m_cond;
      bool m_flag;
      htsmsg_t *m_msg;
    };
  }
}
