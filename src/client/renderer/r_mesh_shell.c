/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
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

#include "r_local.h"

/*
 * @brief Sets the active shell color for the given entity.
 */
static void R_SetMeshShellColor_default(const r_entity_t *e) {
	vec4_t color = { 0.0, 0.0, 0.0, 1.0 };

	VectorCopy(e->shell, color);

	R_Color(color);
}

/*
 * @brief Sets renderer state for the given entity.
 */
static void R_SetMeshShellState_default(const r_entity_t *e) {

	if (e->model->mesh->num_frames == 1) {
		R_SetArrayState(e->model);
	} else {
		R_ResetArrayState();

		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, e->model->texcoords);

		R_InterpolateMeshModel(e);
	}

	R_SetMeshShellColor_default(e);

	R_RotateForEntity(e);

	if (e->effects & EF_WEAPON) {
		glDepthRange(0.0, 0.3);
		glScalef(1.03, 1.03, 1.03);
	} else {
		glScalef(1.125, 1.125, 1.125);
	}
}

/*
 * @brief Restores renderer state for the given entity.
 */
static void R_ResetMeshShellState_default(const r_entity_t *e) {

	if (e->effects & EF_WEAPON)
		glDepthRange(0.0, 1.0);

	R_RotateForEntity(NULL);
}

/*
 * @brief Draws an animated, colored shell for the specified entity.
 */
void R_DrawMeshShell_default(const r_entity_t *e) {

	R_SetMeshShellState_default(e);

	glDrawArrays(GL_TRIANGLES, 0, e->model->num_verts);

	R_ResetMeshShellState_default(e);
}

/*
 * @brief Draws all mesh model shells for the current frame.
 */
void R_DrawMeshShells_default(const r_entities_t *ents) {

	if (!r_shell->value)
		return;

	if (r_draw_wireframe->value)
		return;

	R_EnableShell(r_state.shell_program, true);

	for (size_t i = 0; i < ents->count; i++) {
		const r_entity_t *e = ents->entities[i];

		if ((e->effects & EF_SHELL) == 0)
			continue;

		if (e->effects & EF_NO_DRAW)
			continue;

		r_view.current_entity = e;

		R_DrawMeshShell_default(e);
	}

	r_view.current_entity = NULL;

	R_EnableShell(NULL, false);

	R_Color(NULL);
}
