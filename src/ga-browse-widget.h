/* ga-browse-widget.h
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

#define GA_TYPE_BROWSE_WIDGET (ga_browse_widget_get_type ())
G_DECLARE_FINAL_TYPE (GaBrowseWidget, ga_browse_widget, GA, BROWSE_WIDGET, AdwBin)

GtkWidget *
ga_browse_widget_new (GListModel *model);

void
ga_browse_widget_set_model (GaBrowseWidget *self,
                            GListModel     *model);

GListModel *
ga_browse_widget_get_model (GaBrowseWidget *self);

G_END_DECLS
