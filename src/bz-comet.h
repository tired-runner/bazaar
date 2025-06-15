/* bz-comet.h
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

#define BZ_TYPE_COMET (bz_comet_get_type ())
G_DECLARE_FINAL_TYPE (BzComet, bz_comet, BZ, COMET, GObject)

const char *
bz_comet_get_name (BzComet *self);

GtkWidget *
bz_comet_get_from (BzComet *self);

GtkWidget *
bz_comet_get_to (BzComet *self);

GdkPaintable *
bz_comet_get_paintable (BzComet *self);

GskPath *
bz_comet_get_path (BzComet *self);

double
bz_comet_get_path_length (BzComet *self);

double
bz_comet_get_progress (BzComet *self);

void
bz_comet_set_name (BzComet    *self,
                   const char *name);

void
bz_comet_set_from (BzComet   *self,
                   GtkWidget *from);

void
bz_comet_set_to (BzComet   *self,
                 GtkWidget *to);

void
bz_comet_set_paintable (BzComet      *self,
                        GdkPaintable *paintable);

void
bz_comet_set_path (BzComet *self,
                   GskPath *path);

void
bz_comet_set_path_length (BzComet *self,
                          double   path_length);

void
bz_comet_set_progress (BzComet *self,
                       double   progress);

G_END_DECLS

/* End of bz-comet.h */
