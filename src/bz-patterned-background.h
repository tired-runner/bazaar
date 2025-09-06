/* bz-patterned-background.h
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

#define BZ_TYPE_PATTERNED_BACKGROUND (bz_patterned_background_get_type ())
G_DECLARE_FINAL_TYPE (BzPatternedBackground, bz_patterned_background, BZ, PATTERNED_BACKGROUND, GtkWidget)

BzPatternedBackground *
bz_patterned_background_new (void);

GtkWidget *
bz_patterned_background_get_widget (BzPatternedBackground *self);

void
bz_patterned_background_set_widget (BzPatternedBackground *self,
                                    GtkWidget             *widget);

const char *
bz_patterned_background_get_tint (BzPatternedBackground *self);

void
bz_patterned_background_set_tint (BzPatternedBackground *self,
                                  const char            *tint);

G_END_DECLS

/* End of bz-patterned-background.h */
