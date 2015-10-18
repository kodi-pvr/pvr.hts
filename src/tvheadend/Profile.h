#pragma once

/*
 *      Copyright (C) 2005-2015 Team Kodi
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

#include <string>
#include <vector>

namespace tvheadend
{

  class Profile;
  typedef std::vector<Profile> Profiles;

  /**
   * Represents a single streaming profile
   */
  class Profile
  {
  public:

    std::string GetUuid() const { return m_uuid; }
    void SetUuid(const std::string &uuid) { m_uuid = uuid; }

    std::string GetName() const { return m_name; }
    void SetName(const std::string &name) { m_name = name; }

    std::string GetComment() const { return m_comment; }
    void SetComment(const std::string &comment) { m_comment = comment; }

  private:
    /*
     * The profile UUID
     */
    std::string m_uuid;

    /**
     * The profile name
     */
    std::string m_name;

    /**
     * The profile comment
     */
    std::string m_comment;
  };

}
