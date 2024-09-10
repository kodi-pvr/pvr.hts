/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>

namespace tvheadend::entity
{

/**
 * Abstract entity. An entity can be dirty or clean and has a numeric ID.
 */
class Entity
{
public:
  Entity() = default;
  virtual ~Entity() = default;

  bool operator==(const Entity& right) { return m_id == right.m_id; }

  bool operator!=(const Entity& right) { return !(*this == right); }

  /**
   * @return if the entity is dirty
   */
  virtual bool IsDirty() const { return m_dirty; }

  /**
   * Marks the entity as dirty or not
   * @param dirty The new dirty state
   */
  virtual void SetDirty(bool dirty) { m_dirty = dirty; }

  /**
   * @return the entity ID
   */
  uint32_t GetId() const { return m_id; }

  /**
   * Sets the entity ID
   * @param id The entity id
   */
  void SetId(uint32_t id) { m_id = id; }

protected:
  uint32_t m_id{0};

private:
  bool m_dirty{false};
};

} // namespace tvheadend::entity
