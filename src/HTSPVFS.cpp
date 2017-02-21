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
#include "platform/util/StringUtils.h"
#include "tvheadend/utilities/Logger.h"

extern "C" {
#include "libhts/htsmsg_binary.h"
}

#include "Tvheadend.h"

using namespace std;
using namespace PLATFORM;
using namespace tvheadend::utilities;

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

      {
        CLockObject lock(m_mutex);
        // Check if buffer is empty
        if (m_buffer.avail()) {
          m_bHasData = true;
          m_condition.Broadcast();
        }
      }
    }

    {
      // Take a break, we're either stopped or full
      CLockObject lock(m_mutex);
      m_condition.Wait(m_mutex, 1000);
      if (!m_bHasData)
        m_currentReadLength = MIN_READ_LENGTH;
    }
  }
  return NULL;
}


/*
* VFS handler
*/
CHTSPVFS::CHTSPVFS ( CHTSPConnection &conn )
  : m_conn(conn), m_path(""), m_fileId(0), m_offset(0),
  m_bHasData(false),
  m_bSeekDone(true),
  m_currentReadLength(MAX_READ_LENGTH)
{
  m_buffer.alloc(MAX_BUFFER_SIZE);

  // Start the buffer thread
  m_retry_cnt = 0;
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
    Logger::Log(LogLevel::LEVEL_DEBUG, "vfs re-open file");
    if (!SendFileOpen(true) || !SendFileSeek(m_offset, SEEK_SET, true))
    {
      Logger::Log(LogLevel::LEVEL_ERROR, "vfs failed to re-open file");
      Close();
    }
  }
}

/* **************************************************************************
 * VFS API
 * *************************************************************************/

bool CHTSPVFS::Open ( const PVR_RECORDING &rec )
{
  /* Close existing */
  Close();

  /* Cache details */
  m_path = StringUtils::Format("dvr/%s", rec.strRecordingId);

  /* Send open */
  if (!SendFileOpen())
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "vfs failed to open file");
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
  m_bSeekDone = true;
  m_retry_cnt = 0;
  m_currentReadLength = MIN_READ_LENGTH;
  m_seekCondition.Signal();
}

ssize_t CHTSPVFS::Read ( unsigned char *buf, unsigned int len )
{
  ssize_t ret;
  CLockObject lock(m_mutex);
  /* Not opened */
  if (!m_fileId)
    return -1;

  m_seekCondition.Wait(m_mutex, m_bSeekDone, 3000);

  /* Signal that we need more data in the buffer. Reset the read length to the
     requested length so we don't wait unnecessarily long */
  if (m_buffer.avail() < len)
  {
    m_retry_cnt++;
    m_bHasData = false;
    m_condition.Broadcast();
  }

  /* Wait for data */
  m_condition.Wait(m_mutex, m_bHasData, 3000);

  /* Read */
  ret = m_buffer.read(buf, len);
  m_offset += ret;
  return ret;
}

long long CHTSPVFS::Seek ( long long pos, int whence )
{
  if (m_fileId == 0)
    return -1;

  m_bSeekDone = false;
  return SendFileSeek(pos, whence);
}

long long CHTSPVFS::Tell ( void )
{
  if (m_fileId == 0)
    return -1;

  return m_offset;
}

long long CHTSPVFS::Size ( void )
{
  int64_t ret = -1;
  htsmsg_t *m;
  
  /* Build */
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", m_fileId);

  Logger::Log(LogLevel::LEVEL_TRACE, "vfs stat id=%d", m_fileId);
  
  /* Send */
  {
    CLockObject lock(m_conn.Mutex());
    m = m_conn.SendAndWait("fileStat", m);
  }

  if (m == NULL)
    return -1;

  /* Get size. Note: 'size' field is optional. */
  if (htsmsg_get_s64(m, "size", &ret))
    ret = -1;
  else
    Logger::Log(LogLevel::LEVEL_TRACE, "vfs stat size=%lld", (long long)ret);

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

  Logger::Log(LogLevel::LEVEL_DEBUG, "vfs open file=%s", m_path.c_str());

  /* Send */
  {
    CLockObject lock(m_conn.Mutex());

    if (force)
      m = m_conn.SendAndWait0("fileOpen", m);
    else
      m = m_conn.SendAndWait("fileOpen", m);
  }

  if (m == NULL)
    return false;

  /* Get ID */
  if (htsmsg_get_u32(m, "id", &m_fileId))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed fileOpen response: 'id' missing");
    m_fileId = 0;
  }
  else
    Logger::Log(LogLevel::LEVEL_TRACE, "vfs opened id=%d", m_fileId);

  htsmsg_destroy(m);
  return m_fileId > 0;
}

void CHTSPVFS::SendFileClose ( void )
{
  htsmsg_t *m;

  /* Build */
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", m_fileId);

  Logger::Log(LogLevel::LEVEL_DEBUG, "vfs close id=%d", m_fileId);
  
  /* Send */
  {
    CLockObject lock(m_conn.Mutex());
    m = m_conn.SendAndWait("fileClose", m);
  }

  if (m)
    htsmsg_destroy(m);
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

  Logger::Log(LogLevel::LEVEL_TRACE, "vfs seek id=%d whence=%d pos=%lld",
           m_fileId, whence, (long long)pos);

  /* Send */
  {
    CLockObject lock(m_conn.Mutex());

    if (force)
      m = m_conn.SendAndWait0("fileSeek", m);
    else
      m = m_conn.SendAndWait("fileSeek", m);
  }

  if (m == NULL)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "vfs fileSeek failed");
    return -1;
  }

  /* Get new offset */
  if (htsmsg_get_s64(m, "offset", &ret))
  {
    ret = -1;
    Logger::Log(LogLevel::LEVEL_ERROR, "vfs fileSeek response: 'offset' missing'");

  /* Update */
  }
  else
  {
    Logger::Log(LogLevel::LEVEL_TRACE, "vfs seek offset=%lld", (long long)ret);
    m_offset = ret;
    Reset();
  }

  /* Cleanup */
  htsmsg_destroy(m);

  return ret;
}

bool CHTSPVFS::SendFileRead()
{
  htsmsg_t   *m;
  const void *buf;
  size_t      len;

  /* Build */
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", m_fileId);

  {
    CLockObject lock(m_mutex);
    /* Determine read length */
    m_currentReadLength = (m_currentReadLength > m_buffer.free()) ?
                            m_buffer.free() : m_currentReadLength;
    htsmsg_add_s64(m, "size", m_currentReadLength);
//    printf("%s %lu %u %lu\n", __func__, m_currentReadLength, m_retry_cnt, m_buffer.free());
  }

  Logger::Log(LogLevel::LEVEL_ERROR, "vfs read id=%d size=%d",
    m_fileId, m_currentReadLength);

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
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed fileRead response: 'data' missing");
    return false;
  }


  /* Store */
  if (m_buffer.write((unsigned char*)buf, len) != (ssize_t)len)
  {
    htsmsg_destroy(m);
    Logger::Log(LogLevel::LEVEL_ERROR, "vfs partial buffer write");
    return false;
  }

  {
    /* Gradually increase read length */
    CLockObject lock(m_mutex);

    if (MAX_RETRY_CNT < m_retry_cnt) {
      m_currentReadLength *= 2;
      m_currentReadLength = (m_currentReadLength < MAX_READ_LENGTH) ?
                              m_currentReadLength : MAX_READ_LENGTH;
    }
  }


  htsmsg_destroy(m);
  return true;
}
