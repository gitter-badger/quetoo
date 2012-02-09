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

#include "cg_local.h"

/*
 * Cg_ItemRespawnEffect
 */
static void Cg_ItemRespawnEffect(const vec3_t org) {
	r_particle_t *p;
	r_sustained_light_t s;
	int i, j;

	for (i = 0; i < 64; i++) {

		if (!(p = Cg_AllocParticle()))
			break;

		p->image = cg_particle_spark;
		p->scale_vel = 3.0;

		p->color = 110; // white

		p->org[0] = org[0] + crand() * 8.0;
		p->org[1] = org[1] + crand() * 8.0;
		p->org[2] = org[2] + 8 + frand() * 8.0;

		for (j = 0; j < 2; j++)
			p->vel[j] = crand() * 48.0;
		p->vel[2] = frand() * 48.0;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY * 0.1;

		p->alpha = 1.0;
		p->alpha_vel = -1.5 + frand() * 0.5;
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.9, 0.9, 0.9);
	s.sustain = 1.0;

	cgi.AddSustainedLight(&s);
}

/*
 * Cg_ItemPickupEffect
 */
static void Cg_ItemPickupEffect(const vec3_t org) {
	r_particle_t *p;
	r_sustained_light_t s;
	int i, j;

	for (i = 0; i < 32; i++) {

		if (!(p = Cg_AllocParticle()))
			return;

		p->image = cg_particle_spark;
		p->scale_vel = 3.0;

		p->color = 110; // white

		p->org[0] = org[0] + crand() * 8.0;
		p->org[1] = org[1] + crand() * 8.0;
		p->org[2] = org[2] + 8 + crand() * 16.0;

		for (j = 0; j < 2; j++)
			p->vel[j] = crand() * 16.0;
		p->vel[2] = frand() * 128.0;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = PARTICLE_GRAVITY * 0.2;

		p->alpha = 1.0;
		p->alpha_vel = -1.5 + frand() * 0.5;
	}

	VectorCopy(org, s.light.origin);
	s.light.radius = 80.0;
	VectorSet(s.light.color, 0.9, 1.0, 1.0);
	s.sustain = 1.0;

	cgi.AddSustainedLight(&s);
}

/*
 * Cg_TeleporterEffect
 */
static void Cg_TeleporterEffect(const vec3_t org) {
	Cg_TeleporterTrail(org, NULL);
}

/*
 * Cg_EntityEvent
 */
void Cg_EntityEvent(cl_entity_t *e) {

	entity_state_t *s = &e->current;

	switch (s->event) {
	case EV_ITEM_RESPAWN:
		cgi.PlaySample(NULL, s->number, cg_sample_respawn, ATTN_IDLE);
		Cg_ItemRespawnEffect(s->origin);
		break;
	case EV_ITEM_PICKUP:
		Cg_ItemPickupEffect(s->origin);
		break;
	case EV_TELEPORT:
		cgi.PlaySample(NULL, s->number, cg_sample_teleport, ATTN_IDLE);
		Cg_TeleporterEffect(s->origin);
		break;
	case EV_CLIENT_JUMP:
		cgi.PlaySample(NULL, s->number, cgi.LoadSample(va("*jump_%d", rand() % 5 + 1)), ATTN_NORM);
		break;
	case EV_CLIENT_FOOTSTEP:
		cgi.PlaySample(NULL, s->number, cg_sample_footsteps[rand() & 3], ATTN_NORM);
		break;
	case EV_CLIENT_LAND:
		cgi.PlaySample(NULL, s->number, cgi.LoadSample("*land_1"), ATTN_NORM);
		break;
	case EV_CLIENT_FALL:
		cgi.PlaySample(NULL, s->number, cgi.LoadSample("*fall_2"), ATTN_NORM);
		break;
	case EV_CLIENT_FALL_FAR:
		cgi.PlaySample(NULL, s->number, cgi.LoadSample("*fall_1"), ATTN_NORM);
		break;
	default:
		break;
	}

	s->event = 0;
}