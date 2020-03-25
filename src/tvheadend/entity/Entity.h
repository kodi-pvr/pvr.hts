/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>

namespace tvheadend
{
namespace entity
{

/**
 * Abstract entity. An entity can be dirty or clean and has a numeric ID.
 */
class Entity
{
public:
  Entity() : m_id(0), m_dirty(false){};
  virtual ~Entity() = default;

  /**
   * @return if the entity is dirty
   */
  virtual bool IsDirty() const { return m_dirty; }

  /**
   * Marks the entity as dirty or not
   * @param dirty
   */
  virtual void SetDirty(bool dirty) { m_dirty = dirty; }

  /**
   * @return the entity ID
   */
  uint32_t GetId() const { return m_id; }

  /**
   * Sets the entity ID
   * @param id
   */
  void SetId(uint32_t id) { m_id = id; }

protected:
  uint32_t m_id;

private:
  bool m_dirty;
};

} // namespace entity
} // namespace tvheadend
