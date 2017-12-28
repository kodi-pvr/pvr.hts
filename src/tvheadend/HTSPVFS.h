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

#include <string>

#include "p8-platform/os.h"

struct PVR_RECORDING;

namespace tvheadend
{

class HTSPConnection;

/*
 * HTSP VFS - recordings
 */
class HTSPVFS
{
public:
  HTSPVFS(HTSPConnection &conn);
  ~HTSPVFS();

  void Connected();

  bool Open(const PVR_RECORDING &rec);
  void Close();
  ssize_t Read(unsigned char *buf, unsigned int len);
  long long Seek(long long pos, int whence);
  long long Size();

private:
  bool SendFileOpen(bool force = false);
  void SendFileClose();
  ssize_t SendFileRead(unsigned char *buf, unsigned int len);
  long long SendFileSeek(int64_t pos, int whence, bool force = false);

  HTSPConnection &m_conn;
  std::string m_path;
  uint32_t m_fileId;
  int64_t m_offset;
};

} // namespace tvheadend
