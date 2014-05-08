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

#include "CircBuffer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

CCircBuffer::CCircBuffer(void)
  : m_buffer(NULL), m_alloc(0), m_size(0), m_count(0), m_pin(0), m_pout(0)
{
}

CCircBuffer::~CCircBuffer(void)
{
  unalloc();
}

void CCircBuffer::alloc(size_t size)
{
  if (size > m_alloc) {
    m_alloc  = size;
    m_buffer = (unsigned char*) realloc(m_buffer, size);
  }
  m_size = size;
  reset();
}

void CCircBuffer::unalloc(void)
{
  if(m_buffer)
    ::free(m_buffer);

  m_buffer = NULL;
  m_alloc  = 0;
  m_size   = 0;
  reset();
}

void CCircBuffer::reset(void)
{
  m_pin   = 0;
  m_pout  = 0;
  m_count = 0;
}

size_t CCircBuffer::size(void) const
{
  return m_size;
}

size_t CCircBuffer::avail(void) const
{
  return m_count;
}

size_t CCircBuffer::free(void) const
{
  return m_size - m_count - 1;
}

ssize_t CCircBuffer::write(const unsigned char* data, size_t len)
{
  size_t pt1, pt2;
  if (m_size < 2)
    return -1;
  if (len > free())
    len = free();
  if (m_pin < m_pout)
    memcpy(m_buffer+m_pin, data, len);
  else {
    pt1 = m_size - m_pin;
    if (len < pt1) {
      pt1 = len;
      pt2 = 0;
    } else {
      pt2 = len - pt1;
    }
    memcpy(m_buffer+m_pin, data, pt1);
    memcpy(m_buffer, data+pt1, pt2);
  }
  m_pin    = (m_pin + len) % m_size;
  m_count += len;
  return len;
}

ssize_t CCircBuffer::read(unsigned char* data, size_t len)
{
  size_t pt1, pt2;
  if (m_size < 2)
    return -1;
  if (len > avail())
    len = avail();
  if (m_pout < m_pin)
    memcpy(data, m_buffer+m_pout, len);
  else {
    pt1 = m_size - m_pout;
    if (len < pt1) {
      pt1 = len;
      pt2 = 0;
    } else {
      pt2 = len - pt1;
    }
    memcpy(data, m_buffer+m_pout, pt1);
    memcpy(data+pt1, m_buffer, pt2);
  }
  m_pout   = ((m_pout + m_size) + len) % m_size;
  m_count -= len;
  return len;
}
