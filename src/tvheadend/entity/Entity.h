#pragma once

/*
 *      Copyright (C) 2005-2011 Team XBMC
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

namespace tvheadend
{
  namespace entity
  {

    /**
     * Abstract entity. An entity can be dirty or clean
     */
    class Entity
    {
    public:
      Entity() : m_dirty(false) {};
      virtual ~Entity() = default;

      /**
       * @return if the entity is dirty
       */
      virtual bool IsDirty() const
      {
        return m_dirty;
      }

      /**
       * Marks the entity as dirty or not
       * @param dirty
       */
      virtual void SetDirty(bool dirty)
      {
        m_dirty = dirty;
      }

    private:
      bool m_dirty;
    };
  }
}
