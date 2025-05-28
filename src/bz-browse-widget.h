/* bz-browse-widget.h
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

#include "bz-content-provider.h"

G_BEGIN_DECLS

#define BZ_TYPE_BROWSE_WIDGET (bz_browse_widget_get_type ())
G_DECLARE_FINAL_TYPE (BzBrowseWidget, bz_browse_widget, BZ, BROWSE_WIDGET, AdwBin)

GtkWidget *
bz_browse_widget_new (GListModel *model);

void
bz_browse_widget_set_model (BzBrowseWidget *self,
                            GListModel     *model);

GListModel *
bz_browse_widget_get_model (BzBrowseWidget *self);

void
bz_browse_widget_set_content_provider (BzBrowseWidget    *self,
                                       BzContentProvider *provider);

BzContentProvider *
bz_browse_widget_get_content_provider (BzBrowseWidget *self);

G_END_DECLS
