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

#include "p8-platform/threads/mutex.h"
#include "p8-platform/util/StringUtils.h"

extern "C" {
#include "libhts/htsmsg_binary.h"
}

#include "Tvheadend.h"

using namespace std;
using namespace P8PLATFORM;

/*
* VFS handler
*/
CHTSPVFS::CHTSPVFS ( CHTSPConnection &conn )
  : m_conn(conn), m_path(""), m_fileId(0), m_offset(0)
{
}

CHTSPVFS::~CHTSPVFS ()
{
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
  /* Close existing */
  Close();

  /* Cache details */
  m_path = StringUtils::Format("dvr/%s", rec.strRecordingId);

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
}

ssize_t CHTSPVFS::Read ( unsigned char *buf, unsigned int len )
{
  /* Not opened */
  if (!m_fileId)
    return -1;

  /* Read */
  ssize_t read = SendFileRead(buf, len);

  /* Update */
  if (read > 0)
    m_offset += read;

  return read;
}

long long CHTSPVFS::Seek ( long long pos, int whence )
{
  if (m_fileId == 0)
    return -1;

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

  tvhtrace("vfs stat id=%d", m_fileId);
  
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
    tvherror("malformed fileOpen response: 'id' missing");
    m_fileId = 0;
  }
  else
    tvhtrace("vfs opened id=%d", m_fileId);

  htsmsg_destroy(m);
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

  tvhtrace("vfs seek id=%d whence=%d pos=%lld",
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
    tvherror("vfs fileSeek failed");
    return -1;
  }

  /* Get new offset */
  if (htsmsg_get_s64(m, "offset", &ret))
  {
    ret = -1;
    tvherror("vfs fileSeek response: 'offset' missing'");

  /* Update */
  }
  else
  {
    tvhtrace("vfs seek offset=%lld", (long long)ret);
    m_offset = ret;
  }

  /* Cleanup */
  htsmsg_destroy(m);

  return ret;
}

ssize_t CHTSPVFS::SendFileRead(unsigned char *buf, unsigned int len)
{
  htsmsg_t   *m;
  const void *buffer;
  ssize_t read;

  /* Build */
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", m_fileId);
  htsmsg_add_s64(m, "size", len);

  tvhtrace("vfs read id=%d size=%d",
    m_fileId, len);

  /* Send */
  {
    CLockObject lock(m_conn.Mutex());
    m = m_conn.SendAndWait("fileRead", m);
  }

  if (m == NULL)
  {
    tvherror("vfs fileRead failed");
    return -1;
  }

  /* Get Data */
  if (htsmsg_get_bin(m, "data", &buffer, reinterpret_cast<size_t *>(&read)))
  {
    tvherror("malformed fileRead response: 'data' missing");
    read = -1;

  /* Store */
  }
  else
  {
    memcpy(buf, buffer, read);
  }

  /* Cleanup */
  htsmsg_destroy(m);

  return read;
}
