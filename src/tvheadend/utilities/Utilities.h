/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

namespace tvheadend
{
namespace utilities
{

/**
 * std::remove_if() for maps. Borrowed from:
 * http://stackoverflow.com/questions/800955/remove-if-equivalent-for-stdmap
 */
template<typename ContainerT, typename PredicateT>
void erase_if(ContainerT& items, const PredicateT& predicate)
{
  for (auto it = items.begin(); it != items.end();)
  {
    if (predicate(*it))
      it = items.erase(it);
    else
      ++it;
  }
}

/**
 * Simple hash function. Borrowed from:
 * https://stackoverflow.com/questions/16075271/hashing-a-string-to-an-integer-in-c
 */
static int32_t hash_str_int32(const std::string& str)
{
  int32_t hash = 0x811c9dc5;
  int32_t prime = 0x1000193;

  for (size_t i = 0; i < str.size(); ++i)
  {
    uint8_t value = str[i];
    hash = hash ^ value;
    hash *= prime;
  }

  if (hash < 0)
    hash = -hash;

  return hash;
}

} // namespace utilities
} // namespace tvheadend
