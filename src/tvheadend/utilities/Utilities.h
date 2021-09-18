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
};

} // namespace utilities
} // namespace tvheadend
