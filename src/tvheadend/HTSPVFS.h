/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <memory>
#include <string>

namespace kodi
{
namespace addon
{
class PVRRecording;
}
} // namespace kodi

namespace tvheadend
{

class HTSPConnection;
class Settings;

/*
 * HTSP VFS - recordings
 */
class HTSPVFS
{
public:
  HTSPVFS(const std::shared_ptr<Settings>& settings, HTSPConnection& conn);
  ~HTSPVFS();

  void RebuildState();

  bool Open(const kodi::addon::PVRRecording& rec);
  void Close();
  int64_t Read(unsigned char* buf, unsigned int len, bool inprogress);
  long long Seek(long long pos, int whence, bool inprogress);
  long long Size();
  void PauseStream(bool paused);
  bool IsRealTimeStream();

private:
  bool SendFileOpen(bool force = false);
  void SendFileClose();
  int64_t SendFileRead(unsigned char* buf, unsigned int len);
  long long SendFileSeek(int64_t pos, int whence, bool force = false);

  std::shared_ptr<Settings> m_settings;
  HTSPConnection& m_conn;
  std::string m_path;
  uint32_t m_fileId;
  int64_t m_offset;
  int64_t m_fileStart;
  int64_t m_eofOffsetSecs;
  int64_t m_pauseTime;
  bool m_paused;
  bool m_isRealTimeStream;
};

} // namespace tvheadend
