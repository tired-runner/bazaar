/* bz-entry.h
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BZ_TYPE_ENTRY (bz_entry_get_type ())
G_DECLARE_DERIVABLE_TYPE (BzEntry, bz_entry, BZ, ENTRY, GObject)

struct _BzEntryClass
{
  GObjectClass parent_class;
};

const char *
bz_entry_get_id (BzEntry *self);

const char *
bz_entry_get_unique_id (BzEntry *self);

const char *
bz_entry_get_title (BzEntry *self);

const char *
bz_entry_get_eol (BzEntry *self);

const char *
bz_entry_get_description (BzEntry *self);

const char *
bz_entry_get_long_description (BzEntry *self);

const char *
bz_entry_get_remote_repo_name (BzEntry *self);

guint64
bz_entry_get_size (BzEntry *self);

GdkPaintable *
bz_entry_get_icon_paintable (BzEntry *self);

GPtrArray *
bz_entry_get_search_tokens (BzEntry *self);

gint
bz_entry_cmp_usefulness (gconstpointer a,
                         gconstpointer b,
                         gpointer      user_data);

G_END_DECLS
