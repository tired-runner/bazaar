/* ga-search-widget.h
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

#define GA_TYPE_SEARCH_WIDGET (ga_search_widget_get_type ())
G_DECLARE_FINAL_TYPE (GaSearchWidget, ga_search_widget, GA, SEARCH_WIDGET, AdwBin)

GtkWidget *
ga_search_widget_new (GListModel *model);

void
ga_search_widget_set_model (GaSearchWidget *self,
                            GListModel     *model);

GListModel *
ga_search_widget_get_model (GaSearchWidget *self);

gpointer
ga_search_widget_get_selected (GaSearchWidget *self);

gpointer
ga_search_widget_get_previewing (GaSearchWidget *self);

G_END_DECLS
