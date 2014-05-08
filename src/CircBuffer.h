#pragma once

/*
 *      Copyright (C) 2005-2012 Team XBMC
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
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301  USA
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "platform/os.h"

class CCircBuffer
{
public:
  CCircBuffer    (void);
  ~CCircBuffer   (void);

  void    alloc   (size_t);
  void    unalloc (void);
  void    reset   (void);

  size_t  size   (void) const;
  size_t  avail  (void) const;
  size_t  free   (void) const;

  ssize_t write  (const unsigned char* data, size_t len);
  ssize_t read   (unsigned char* data, size_t len);

protected:
  unsigned char * m_buffer;
  size_t m_alloc;
  size_t m_size;
  size_t m_count;
  size_t m_pin;
  size_t m_pout;

};
