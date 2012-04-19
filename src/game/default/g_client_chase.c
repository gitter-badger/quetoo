/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "g_local.h"

/*
 * G_ClientChaseThink
 */
void G_ClientChaseThink(g_edict_t *ent) {
	const g_edict_t *targ;

	targ = ent->client->chase_target;

	// copy origin
	VectorCopy(targ->s.origin, ent->s.origin);

	// and angles
	VectorCopy(targ->client->angles, ent->client->angles);
	PackAngles(targ->client->angles, ent->client->ps.pm_state.view_angles);

	if (targ->dead) { // drop view towards the floor
		ent->client->ps.pm_state.pm_flags |= PMF_DUCKED;
	}

	// disable the spectator's input
	ent->client->ps.pm_state.pm_type = PM_FREEZE;

	// disable client prediction
	ent->client->ps.pm_state.pm_flags |= PMF_NO_PREDICTION;

	gi.LinkEntity(ent);
}

/*
 * G_ClientChaseNext
 */
void G_ClientChaseNext(g_edict_t *ent) {
	int i;
	g_edict_t *e;

	if (!ent->client->chase_target)
		return;

	i = ent->client->chase_target - g_game.edicts;
	do {
		i++;

		if (i > sv_max_clients->integer)
			i = 1;

		e = g_game.edicts + i;

		if (!e->in_use)
			continue;

		if (!e->client->persistent.spectator)
			break;

	} while (e != ent->client->chase_target);

	ent->client->chase_target = e;
}

/*
 * G_ClientChasePrevious
 */
void G_ClientChasePrevious(g_edict_t *ent) {
	int i;
	g_edict_t *e;

	if (!ent->client->chase_target)
		return;

	i = ent->client->chase_target - g_game.edicts;
	do {
		i--;

		if (i < 1)
			i = sv_max_clients->integer;

		e = g_game.edicts + i;

		if (!e->in_use)
			continue;

		if (!e->client->persistent.spectator)
			break;

	} while (e != ent->client->chase_target);

	ent->client->chase_target = e;
}

/*
 * G_ClientChaseTarget
 *
 * Finds the first available chase target and assigns it to the specified ent.
 */
void G_ClientChaseTarget(g_edict_t *ent) {
	int i;
	g_edict_t *other;

	for (i = 1; i <= sv_max_clients->integer; i++) {
		other = g_game.edicts + i;
		if (other->in_use && !other->client->persistent.spectator) {
			ent->client->chase_target = other;
			G_ClientChaseThink(ent);
			return;
		}
	}
}

