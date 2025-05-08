/* ga-flatpak-private.h
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

#include <flatpak.h>

#include "ga-flatpak-entry.h"
#include "ga-flatpak-instance.h"

G_BEGIN_DECLS

/* GaFlatpakInstance */

FlatpakInstallation *
ga_flatpak_instance_get_installation (GaFlatpakInstance *self);

/* GaFlatpakEntry */

GaFlatpakEntry *
ga_flatpak_entry_new_for_remote_ref (GaFlatpakInstance *instance,
                                     FlatpakRemoteRef  *rref,
                                     GError           **error);

FlatpakRef *
ga_flatpak_entry_get_ref (GaFlatpakEntry *self);

G_END_DECLS
