/* bz-paintable-model.h
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
#include <libdex.h>

G_BEGIN_DECLS

#define BZ_TYPE_PAINTABLE_MODEL (bz_paintable_model_get_type ())
G_DECLARE_FINAL_TYPE (BzPaintableModel, bz_paintable_model, BZ, PAINTABLE_MODEL, GObject)

BzPaintableModel *
bz_paintable_model_new (GListModel *model);

void
bz_paintable_model_set_model (BzPaintableModel *self,
                              GListModel       *model);

GListModel *
bz_paintable_model_get_model (BzPaintableModel *self);

gboolean
bz_paintable_model_is_fully_loaded (BzPaintableModel *self);

G_END_DECLS
