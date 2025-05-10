/* ga-flatpak-entry.h
 *
 * Copyright 2025 Adam Masciola
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ga-entry.h"

G_BEGIN_DECLS

#define GA_TYPE_FLATPAK_ENTRY (ga_flatpak_entry_get_type ())
G_DECLARE_FINAL_TYPE (GaFlatpakEntry, ga_flatpak_entry, GA, FLATPAK_ENTRY, GaEntry)

const char *
ga_flatpak_entry_get_name (GaFlatpakEntry *self);

gboolean
ga_flatpak_entry_launch (GaFlatpakEntry *self,
                         GError        **error);

G_END_DECLS
