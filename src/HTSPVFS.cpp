/*
 *      Copyright (C) 2014 Adam Sutton
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

#include "platform/threads/mutex.h"
#include "platform/util/timeutils.h"
#include "platform/sockets/tcp.h"

extern "C" {
#include "platform/util/atomic.h"
#include "libhts/htsmsg_binary.h"
#include "libhts/sha1.h"
}

#include "Tvheadend.h"
#include "client.h"

using namespace std;
using namespace ADDON;
using namespace PLATFORM;

/*
* The buffer thread
*/
void *CHTSPVFS::Process(void)
{
  while (!IsStopped())
  {
    while (m_fileId && m_buffer.free() > 0)
    {
      if (!SendFileRead())
        continue;
       
      CLockObject lock(m_mutex);
      m_bHasData = true;
      m_condition.Broadcast();
    }

    // Take a break, we're either stopped or full
    CLockObject lock(m_mutex);
    m_condition.Wait(m_mutex, 1000);
  }

  return NULL;
}

/*
* VFS handler
*/
CHTSPVFS::CHTSPVFS ( CHTSPConnection &conn )
  : m_conn(conn), m_path(""), m_fileId(0), m_offset(0), 
  m_currentReadLength(INITAL_READ_LENGTH)
{
  m_buffer.alloc(MAX_BUFFER_SIZE);

  // Start the buffer thread
  CreateThread();
}

CHTSPVFS::~CHTSPVFS ( void )
{
  // Stop the buffer thread
  StopThread();
}

void CHTSPVFS::Connected ( void )
{
  /* Re-open */
  if (m_fileId != 0)
  { 
    tvhdebug("vfs re-open file");
    if (!SendFileOpen(true) || !SendFileSeek(m_offset, SEEK_SET, true))
    {
      tvherror("vfs failed to re-open file");
      Close();
    }
  }
}

/* **************************************************************************
 * VFS API
 * *************************************************************************/

bool CHTSPVFS::Open ( const PVR_RECORDING &rec )
{
  CLockObject lock(m_conn.Mutex());

  /* Close existing */
  Close();

  /* Cache details */
  m_path.Format("dvr/%s", rec.strRecordingId);

  /* Send open */
  if (!SendFileOpen())
  {
    tvherror("vfs failed to open file");
    return false;
  }

  /* Done */
  return true;
}

void CHTSPVFS::Close ( void )
{
  if (m_fileId != 0)
    SendFileClose();

  m_offset = 0;
  m_fileId = 0;
  m_path   = "";

  Reset();
}

void CHTSPVFS::Reset()
{
  CLockObject lock(m_mutex);
  m_buffer.reset();
  m_bHasData = false;
  m_currentReadLength = INITAL_READ_LENGTH;
}

int CHTSPVFS::Read ( unsigned char *buf, unsigned int len )
{
  ssize_t ret;

  /* Not opened */
  if (!m_fileId)
    return -1;

  /* Signal that we need more data in the buffer. Reset the read length to the 
     requested length so we don't wait unnecessarily long */
  if (m_buffer.avail() < len)
  {
    CLockObject lock(m_mutex);
    m_bHasData = false;
    m_currentReadLength = len;
    m_condition.Broadcast();
  }

  /* Wait for data */
  CLockObject lock(m_mutex);
  m_condition.Wait(m_mutex, m_bHasData, 5000);

  /* Read */
  ret = m_buffer.read(buf, len);
  m_offset += ret;
  return (int)ret;
}

long long CHTSPVFS::Seek ( long long pos, int whence )
{
  CLockObject lock(m_conn.Mutex());
  if (m_fileId == 0)
    return -1;
  return SendFileSeek(pos, whence);
}

long long CHTSPVFS::Tell ( void )
{
  CLockObject lock(m_conn.Mutex());
  if (m_fileId == 0)
    return -1;
  return m_offset;
}

long long CHTSPVFS::Size ( void )
{
  int64_t ret = -1;
  CLockObject lock(m_conn.Mutex());
  htsmsg_t *m;
  
  /* Build */
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", m_fileId);

  tvhtrace("vfs stat id=%d", m_fileId);
  
  /* Send */
  if ((m = m_conn.SendAndWait("fileStat", m)) == NULL)
    return -1;

  /* Process */
  if (htsmsg_get_s64(m, "size", &ret))
    tvherror("vfs fileStat malformed response");
  else
    tvhtrace("vfs stat size=%lld", (long long)ret);
  htsmsg_destroy(m);

  return ret;
}

/* **************************************************************************
 * HTSP Messages
 * *************************************************************************/

bool CHTSPVFS::SendFileOpen ( bool force )
{
  htsmsg_t *m;

  /* Build Message */
  m = htsmsg_create_map();
  htsmsg_add_str(m, "file", m_path.c_str());

  tvhdebug("vfs open file=%s", m_path.c_str());

  /* Send */
  if (force)
    m = m_conn.SendAndWait0("fileOpen", m);
  else
    m = m_conn.SendAndWait("fileOpen", m);
  if (m == NULL)
    return false;

  /* Get ID */
  htsmsg_get_u32(m, "id", &m_fileId);
  htsmsg_destroy(m);
  tvhtrace("vfs opened id=%d", m_fileId);

  /* Log */
  return m_fileId > 0;
}

void CHTSPVFS::SendFileClose ( void )
{
  htsmsg_t *m;

  /* Build */
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", m_fileId);

  tvhdebug("vfs close id=%d", m_fileId);
  
  /* Send */
  m = m_conn.SendAndWait("fileClose", m);
  if (m)
    htsmsg_destroy(m);
  // Note: ignore the return;
}

long long CHTSPVFS::SendFileSeek ( int64_t pos, int whence, bool force )
{
  htsmsg_t *m;
  int64_t ret = -1;

  /* Build Message */
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "id",     m_fileId);
  htsmsg_add_s64(m, "offset", pos);
  if (whence == SEEK_CUR)
    htsmsg_add_str(m, "whence", "SEEK_CUR");
  else if (whence == SEEK_END)
    htsmsg_add_str(m, "whence", "SEEK_END");

  tvhtrace("vfs seek id=%d whence=%d pos=%lld",
           m_fileId, whence, (long long)pos);

  /* Send */
  if (force)
    m = m_conn.SendAndWait0("fileSeek", m);
  else
    m = m_conn.SendAndWait("fileSeek", m);
  if (m == NULL)
    return false;

  /* Get new offset */
  if (htsmsg_get_s64(m, "offset", &ret))
    tvherror("vfs malformed fileSeek response");
  htsmsg_destroy(m);
  
  /* Update */
  if (ret >= 0)
  {
    tvhtrace("vfs seek offset=%lld", (long long)ret);
    m_offset = ret;
    
    Reset();
  }
  else
    tvherror("vfs fileSeek failed");

  return ret;
}

bool CHTSPVFS::SendFileRead()
{
  htsmsg_t   *m;
  const void *buf;
  size_t      len;
  size_t      readLength;

  {
    CLockObject lock(m_mutex);

    /* Determine read length */
    if (m_currentReadLength > m_buffer.free())
      readLength = m_buffer.free();
    else
      readLength = m_currentReadLength;
  }

  /* Build */
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", m_fileId);
  htsmsg_add_s64(m, "size", readLength);

  tvhtrace("vfs read id=%d size=%d",
    m_fileId, readLength);

  /* Send */
  {
    CLockObject lock(m_conn.Mutex());
    m = m_conn.SendAndWait("fileRead", m);
  }

  if (m == NULL)
    return false;

  /* Process */
  if (htsmsg_get_bin(m, "data", &buf, &len))
  {
    htsmsg_destroy(m);
    tvherror("vfs fileRead malformed response");
    return false;
  }

  /* Store */
  if (m_buffer.write((unsigned char*)buf, len) != (ssize_t)len)
  {
    htsmsg_destroy(m);
    tvherror("vfs partial buffer write");
    return false;
  }

  /* Gradually increase read length */
  CLockObject lock(m_mutex);

  if (m_currentReadLength * 2 < MAX_READ_LENGTH)
    m_currentReadLength *= 2;

  htsmsg_destroy(m);
  return true;
}
