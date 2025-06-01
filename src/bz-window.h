/* bz-window.h
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

#include <adwaita.h>

G_BEGIN_DECLS

#define BZ_TYPE_WINDOW (bz_window_get_type ())
G_DECLARE_FINAL_TYPE (BzWindow, bz_window, BZ, WINDOW, AdwApplicationWindow)

void
bz_window_refresh (BzWindow *self);

void
bz_window_browse (BzWindow *self);

void
bz_window_search (BzWindow   *self,
                  const char *text);

void
bz_window_toggle_transactions (BzWindow *self);

G_END_DECLS
