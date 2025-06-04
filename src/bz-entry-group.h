/* bz-entry-group.h
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

#include "bz-entry.h"

G_BEGIN_DECLS

#define BZ_TYPE_ENTRY_GROUP (bz_entry_group_get_type ())
G_DECLARE_FINAL_TYPE (BzEntryGroup, bz_entry_group, BZ, ENTRY_GROUP, GObject)

BzEntryGroup *
bz_entry_group_new (void);

GListModel *
bz_entry_group_get_model (BzEntryGroup *self);

BzEntry *
bz_entry_group_get_ui_entry (BzEntryGroup *self);

void
bz_entry_group_add (BzEntryGroup *self,
                    BzEntry      *entry,
                    gboolean      installable,
                    gboolean      updatable,
                    gboolean      removable);

void
bz_entry_group_install (BzEntryGroup *self,
                        BzEntry      *entry);

void
bz_entry_group_remove (BzEntryGroup *self,
                       BzEntry      *entry);

gboolean
bz_entry_group_query_installable (BzEntryGroup *self,
                                  BzEntry      *entry);

gboolean
bz_entry_group_query_updatable (BzEntryGroup *self,
                                BzEntry      *entry);

gboolean
bz_entry_group_query_removable (BzEntryGroup *self,
                                BzEntry      *entry);

G_END_DECLS
