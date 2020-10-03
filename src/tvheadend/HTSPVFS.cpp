/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "HTSPVFS.h"

extern "C"
{
#include "libhts/htsmsg_binary.h"
}
#include "HTSPConnection.h"
#include "Settings.h"
#include "utilities/Logger.h"

#include "kodi/addon-instance/pvr/Recordings.h"
#include "p8-platform/util/StringUtils.h"

#include <chrono>
#include <ctime>
#include <mutex>
#include <thread>

using namespace tvheadend;
using namespace tvheadend::utilities;

/*
 * VFS handler
 */
HTSPVFS::HTSPVFS(HTSPConnection& conn)
  : m_conn(conn),
    m_path(""),
    m_fileId(0),
    m_offset(0),
    m_eofOffsetSecs(-1),
    m_pauseTime(0),
    m_paused(false),
    m_isRealTimeStream(false)
{
}

HTSPVFS::~HTSPVFS()
{
}

void HTSPVFS::RebuildState()
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

bool HTSPVFS::Open(const kodi::addon::PVRRecording& rec)
{
  /* Close existing */
  Close();

  /* Cache details */
  m_path = StringUtils::Format("dvr/%s", rec.GetRecordingId().c_str());
  m_fileStart = rec.GetRecordingTime();

  /* Send open */
  if (!SendFileOpen())
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "vfs failed to open file");
    return false;
  }

  /* Done */
  return true;
}

void HTSPVFS::Close()
{
  if (m_fileId != 0)
    SendFileClose();

  m_offset = 0;
  m_fileId = 0;
  m_path.clear();
  m_eofOffsetSecs = -1;
  m_pauseTime = 0;
  m_paused = false;
  m_isRealTimeStream = false;
}

ssize_t HTSPVFS::Read(unsigned char* buf, unsigned int len, bool inprogress)
{
  /* Not opened */
  if (!m_fileId)
    return -1;

  /* Tvheadend may briefly return 0 bytes when playing an in-progress recording at end-of-file
     we'll retry 50 times with 10ms pauses (~500ms) before giving up */
  int tries = inprogress ? 50 : 1;
  ssize_t read = 0;

  for (int i = 1; i <= tries; i++)
  {
    read = SendFileRead(buf, len);
    if (read > 0)
    {
      m_offset += read;
      return read;
    }
    else if (i < tries)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  Logger::Log(LogLevel::LEVEL_DEBUG, "vfs read failed after %d attempts", tries);
  return read;
}

long long HTSPVFS::Seek(long long pos, int whence, bool inprogress)
{
  if (m_fileId == 0)
    return -1;

  long long ret = SendFileSeek(pos, whence);

  /* for inprogress recordings see whether we need to toggle IsRealTimeStream */
  if (inprogress)
  {
    int64_t fileLengthSecs = std::time(nullptr) - m_fileStart;
    int64_t fileSize = Size();
    int64_t bitrate = 0;
    m_eofOffsetSecs = -1;

    if (fileLengthSecs > 0)
      bitrate = fileSize / fileLengthSecs;

    if (bitrate > 0)
      m_eofOffsetSecs = (fileSize - m_offset) > 0 ? (fileSize - m_offset) / bitrate : 0;

    /* only set m_isRealTimeStream within last 10 secs of an inprogress recording */
    m_isRealTimeStream = (m_eofOffsetSecs >= 0 && m_eofOffsetSecs < 10);
    Logger::Log(LogLevel::LEVEL_TRACE,
                "vfs seek inprogress recording m_eofOffsetSecs=%lld m_isRealTimeStream=%d",
                static_cast<long long>(m_eofOffsetSecs), m_isRealTimeStream);

    /* if we've seeked whilst paused then update m_pauseTime
       this is so we correctly recalculate m_eofOffsetSecs when returning to normal playback */
    if (m_paused)
      m_pauseTime = std::time(nullptr);
  }

  return ret;
}

long long HTSPVFS::Size()
{
  int64_t ret = -1;

  /* Build */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", m_fileId);

  Logger::Log(LogLevel::LEVEL_TRACE, "vfs stat id=%d", m_fileId);

  /* Send */
  {
    std::unique_lock<std::recursive_mutex> lock(m_conn.Mutex());
    m = m_conn.SendAndWait(lock, "fileStat", m);
  }

  if (!m)
    return -1;

  /* Get size. Note: 'size' field is optional. */
  if (htsmsg_get_s64(m, "size", &ret))
    ret = -1;
  else
    Logger::Log(LogLevel::LEVEL_TRACE, "vfs stat size=%lld", static_cast<long long>(ret));

  htsmsg_destroy(m);

  return ret;
}

void HTSPVFS::PauseStream(bool paused)
{
  m_paused = paused;

  if (paused)
  {
    m_pauseTime = std::time(nullptr);
  }
  else
  {
    if (m_eofOffsetSecs >= 0 && m_pauseTime > 0)
    {
      /* correct m_eofOffsetSecs based on how long we've been paused */
      m_eofOffsetSecs += (std::time(nullptr) - m_pauseTime);
      m_isRealTimeStream = (m_eofOffsetSecs >= 0 && m_eofOffsetSecs < 10);
      Logger::Log(LogLevel::LEVEL_TRACE,
                  "vfs unpause inprogress recording m_eofOffsetSecs=%lld m_isRealTimeStream=%d",
                  static_cast<long long>(m_eofOffsetSecs), m_isRealTimeStream);
    }
    m_pauseTime = 0;
  }
}

bool HTSPVFS::IsRealTimeStream()
{
  return m_isRealTimeStream;
}

/* **************************************************************************
 * HTSP Messages
 * *************************************************************************/

bool HTSPVFS::SendFileOpen(bool force)
{
  /* Build Message */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_str(m, "file", m_path.c_str());

  Logger::Log(LogLevel::LEVEL_DEBUG, "vfs open file=%s", m_path.c_str());

  /* Send */
  {
    std::unique_lock<std::recursive_mutex> lock(m_conn.Mutex());

    if (force)
      m = m_conn.SendAndWait0(lock, "fileOpen", m);
    else
      m = m_conn.SendAndWait(lock, "fileOpen", m);
  }

  if (!m)
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

void HTSPVFS::SendFileClose()
{
  /* Build */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", m_fileId);

  /* If setting set, we will increase play count with CTvheadend::SetPlayCount */
  if (m_conn.GetProtocol() >= 27)
    htsmsg_add_u32(m, "playcount",
                   Settings::GetInstance().GetDvrPlayStatus() ? HTSP_DVR_PLAYCOUNT_KEEP
                                                              : HTSP_DVR_PLAYCOUNT_INCR);

  Logger::Log(LogLevel::LEVEL_DEBUG, "vfs close id=%d", m_fileId);

  /* Send */
  {
    std::unique_lock<std::recursive_mutex> lock(m_conn.Mutex());
    m = m_conn.SendAndWait(lock, "fileClose", m);
  }

  if (m)
    htsmsg_destroy(m);
}

long long HTSPVFS::SendFileSeek(int64_t pos, int whence, bool force)
{
  int64_t ret = -1;

  /* Build Message */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", m_fileId);
  htsmsg_add_s64(m, "offset", pos);
  if (whence == SEEK_CUR)
    htsmsg_add_str(m, "whence", "SEEK_CUR");
  else if (whence == SEEK_END)
    htsmsg_add_str(m, "whence", "SEEK_END");

  Logger::Log(LogLevel::LEVEL_TRACE, "vfs seek id=%d whence=%d pos=%lld", m_fileId, whence,
              static_cast<long long>(pos));

  /* Send */
  {
    std::unique_lock<std::recursive_mutex> lock(m_conn.Mutex());

    if (force)
      m = m_conn.SendAndWait0(lock, "fileSeek", m);
    else
      m = m_conn.SendAndWait(lock, "fileSeek", m);
  }

  if (!m)
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
    Logger::Log(LogLevel::LEVEL_TRACE, "vfs seek offset=%lld", static_cast<long long>(ret));
    m_offset = ret;
  }

  /* Cleanup */
  htsmsg_destroy(m);

  return ret;
}

ssize_t HTSPVFS::SendFileRead(unsigned char* buf, unsigned int len)
{
  /* Build */
  htsmsg_t* m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", m_fileId);
  htsmsg_add_s64(m, "size", len);

  Logger::Log(LogLevel::LEVEL_TRACE, "vfs read id=%d size=%d", m_fileId, len);

  /* Send */
  {
    std::unique_lock<std::recursive_mutex> lock(m_conn.Mutex());
    m = m_conn.SendAndWait(lock, "fileRead", m);
  }

  if (!m)
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "vfs fileRead failed");
    return -1;
  }

  /* Get Data */
  const void* buffer = nullptr;
  size_t read = 0;
  if (htsmsg_get_bin(m, "data", &buffer, &read))
  {
    Logger::Log(LogLevel::LEVEL_ERROR, "malformed fileRead response: 'data' missing");
    return -1;
    /* Store */
  }
  else
  {
    std::memcpy(buf, buffer, read);
  }

  /* Cleanup */
  htsmsg_destroy(m);

  return read;
}
