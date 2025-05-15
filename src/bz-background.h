/* bz-background.h
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

#define BZ_TYPE_BACKGROUND (bz_background_get_type ())
G_DECLARE_FINAL_TYPE (BzBackground, bz_background, BZ, BACKGROUND, GtkWidget)

GtkWidget *
bz_background_new (void);

void
bz_background_set_entries (BzBackground *self,
                           GListModel   *entries);

GListModel *
bz_background_get_entries (BzBackground *self);

void
bz_background_set_motion_controller (BzBackground             *self,
                                     GtkEventControllerMotion *controller);

GtkEventControllerMotion *
bz_background_get_motion_controller (BzBackground *self);

G_END_DECLS
