/* ga-background.h
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

#define GA_TYPE_BACKGROUND (ga_background_get_type ())
G_DECLARE_FINAL_TYPE (GaBackground, ga_background, GA, BACKGROUND, GtkWidget)

GtkWidget *
ga_background_new (void);

void
ga_background_set_icons (GaBackground *self,
                         GListModel   *icons);

GListModel *
ga_background_get_icons (GaBackground *self);

void
ga_background_set_motion_controller (GaBackground             *self,
                                     GtkEventControllerMotion *controller);

GtkEventControllerMotion *
ga_background_get_motion_controller (GaBackground *self);

G_END_DECLS
