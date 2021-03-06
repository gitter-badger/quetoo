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

r_mesh_state_t r_mesh_state;

/*
 * @brief Applies any client-side transformations specified by the model's world or
 * view configuration structure.
 */
void R_ApplyMeshModelConfig(r_entity_t *e) {
	const r_mesh_config_t *c;
	vec3_t translate;
	int32_t i;

	// translation is applied differently for view weapons
	if (e->effects & EF_WEAPON) {

		// apply weapon bob on all 3 axis
		vec_t b = r_view.bob * 0.4;

		c = e->model->mesh->view_config;

		VectorMA(e->origin, c->translate[0] + b, r_view.forward, e->origin);
		VectorMA(e->origin, 6.0, r_view.right, e->origin);

		b = r_view.bob * 0.25;

		VectorMA(e->origin, c->translate[1] + b, r_view.right, e->origin);
		VectorMA(e->origin, c->translate[2] + b, r_view.up, e->origin);
	} else { // versus world and linked entities

		if (e->parent)
			c = e->model->mesh->link_config;
		else
			c = e->model->mesh->world_config;

		// normalize the config's translation to the entity scale
		for (i = 0; i < 3; i++)
			translate[i] = c->translate[i] * e->scale;

		// and add it to the origin
		VectorAdd(e->origin, translate, e->origin);
	}

	// apply scale
	e->scale *= c->scale;

	// lastly apply effects
	e->effects |= c->flags;
}

/*
 * @brief Returns the desired tag structure, or NULL.
 */
static const r_md3_tag_t *R_GetMeshModelTag(const r_model_t *mod, int32_t frame, const char *name) {

	if (frame > mod->mesh->num_frames) {
		Com_Warn("%s: Invalid frame: %d\n", mod->media.name, frame);
		return NULL;
	}

	const r_md3_t *md3 = (r_md3_t *) mod->mesh->data;
	const r_md3_tag_t *tag = &md3->tags[frame * md3->num_tags];
	int32_t i;

	for (i = 0; i < md3->num_tags; i++, tag++) {
		if (!g_strcmp0(name, tag->name)) {
			return tag;
		}
	}

	Com_Warn("%s: Tag not found: %s\n", mod->media.name, name);
	return NULL;
}

/*
 * @brief Applies transformation and rotation for the specified linked entity.
 */
void R_ApplyMeshModelTag(r_entity_t *e) {

	if (!e->parent || !e->parent->model || e->parent->model->type != MOD_MD3) {
		Com_Warn("Invalid parent entity\n");
		return;
	}

	if (!e->tag_name) {
		Com_Warn("NULL tag_name\n");
		return;
	}

	// interpolate the tag over the frames of the parent entity

	const r_md3_tag_t *t1 = R_GetMeshModelTag(e->parent->model, e->parent->old_frame, e->tag_name);
	const r_md3_tag_t *t2 = R_GetMeshModelTag(e->parent->model, e->parent->frame, e->tag_name);

	if (!t1 || !t2) {
		return;
	}

	matrix4x4_t local, lerped, normalized;

	Matrix4x4_Concat(&local, &e->parent->matrix, &e->matrix);

	Matrix4x4_Interpolate(&lerped, &t2->matrix, &t1->matrix, e->parent->back_lerp);
	Matrix4x4_Normalize(&normalized, &lerped);

	Matrix4x4_Concat(&e->matrix, &local, &normalized);

	//Com_Debug("%s: %3.2f %3.2f %3.2f\n", e->tag_name,
	//		e->matrix.m[0][3], e->matrix.m[1][3], e->matrix.m[2][3]);
}

/*
 * @return True if the specified entity was frustum-culled and can be skipped.
 */
_Bool R_CullMeshModel(const r_entity_t *e) {
	vec3_t mins, maxs;

	if (e->effects & EF_WEAPON) // never cull the weapon
		return false;

	// calculate scaled bounding box in world space

	VectorMA(e->origin, e->scale, e->model->mins, mins);
	VectorMA(e->origin, e->scale, e->model->maxs, maxs);

	return R_CullBox(mins, maxs);
}

/*
 * @brief Updates static lighting information for the specified mesh entity.
 */
void R_UpdateMeshModelLighting(const r_entity_t *e) {

	if (e->effects & EF_NO_LIGHTING)
		return;

	if (e->lighting->state != LIGHTING_READY) {

		// update the origin and bounds based on the entity
		if (e->effects & EF_WEAPON)
			VectorCopy(r_view.origin, e->lighting->origin);
		else
			VectorCopy(e->origin, e->lighting->origin);

		e->lighting->radius = e->scale * e->model->radius;

		// calculate scaled bounding box in world space
		VectorMA(e->lighting->origin, e->scale, e->model->mins, e->lighting->mins);
		VectorMA(e->lighting->origin, e->scale, e->model->maxs, e->lighting->maxs);
	}

	R_UpdateLighting(e->lighting);
}

/*
 * @brief Interpolate the animation for the give entity, writing primitives to
 * the default vertex arrays. This must be called at each frame for animated
 * entities.
 */
void R_InterpolateMeshModel(const r_entity_t *e) {

	const r_md3_t *md3 = (r_md3_t *) e->model->mesh->data;
	const d_md3_frame_t *frame, *old_frame;

	if (e->frame >= e->model->mesh->num_frames) {
		Com_Warn("%s: no such frame %d\n", e->model->media.name, e->frame);
		frame = &md3->frames[0];
	} else {
		frame = &md3->frames[e->frame];
	}

	if (e->old_frame >= e->model->mesh->num_frames) {
		Com_Warn("%s: no such old_frame %d\n", e->model->media.name, e->old_frame);
		old_frame = &md3->frames[0];
	} else {
		old_frame = &md3->frames[e->old_frame];
	}

	vec3_t trans;
	for (int32_t i = 0; i < 3; i++) // calculate the translation
		trans[i] = e->back_lerp * old_frame->translate[i] + e->lerp * frame->translate[i];

	GLuint vert_index = 0;

	const r_md3_mesh_t *mesh = md3->meshes;
	for (uint16_t i = 0; i < md3->num_meshes; i++, mesh++) { // iterate the meshes

		const r_md3_vertex_t *v = mesh->verts + e->frame * mesh->num_verts;
		const r_md3_vertex_t *ov = mesh->verts + e->old_frame * mesh->num_verts;

		const uint32_t *tri = mesh->tris;

		for (uint16_t j = 0; j < mesh->num_verts; j++, v++, ov++) { // interpolate the vertexes
			VectorSet(r_mesh_state.vertexes[j],
					trans[0] + ov->point[0] * e->back_lerp + v->point[0] * e->lerp,
					trans[1] + ov->point[1] * e->back_lerp + v->point[1] * e->lerp,
					trans[2] + ov->point[2] * e->back_lerp + v->point[2] * e->lerp);

			if (r_state.lighting_enabled) { // and the normals
				VectorSet(r_mesh_state.normals[j],
						v->normal[0] + (ov->normal[0] - v->normal[0]) * e->back_lerp,
						v->normal[1] + (ov->normal[1] - v->normal[1]) * e->back_lerp,
						v->normal[2] + (ov->normal[2] - v->normal[2]) * e->back_lerp);
			}
		}

		for (uint16_t j = 0; j < mesh->num_tris; j++, tri += 3) { // populate the triangles

			VectorCopy(r_mesh_state.vertexes[tri[0]], (&r_state.vertex_array_3d[vert_index + 0]));
			VectorCopy(r_mesh_state.vertexes[tri[1]], (&r_state.vertex_array_3d[vert_index + 3]));
			VectorCopy(r_mesh_state.vertexes[tri[2]], (&r_state.vertex_array_3d[vert_index + 6]));

			if (r_state.lighting_enabled) { // normal vectors for lighting
				VectorCopy(r_mesh_state.normals[tri[0]], (&r_state.normal_array[vert_index + 0]));
				VectorCopy(r_mesh_state.normals[tri[1]], (&r_state.normal_array[vert_index + 3]));
				VectorCopy(r_mesh_state.normals[tri[2]], (&r_state.normal_array[vert_index + 6]));
			}

			vert_index += 9;
		}
	}
}

/*
 * @brief Sets the shade color for the mesh by modulating any preset color
 * with static lighting.
 */
static void R_SetMeshColor_default(const r_entity_t *e) {
	vec4_t color;

	VectorCopy(r_bsp_light_state.ambient, color);

	if (!r_lighting->value || !r_programs->value) {
		const r_illumination_t *il = e->lighting->illuminations;

		for (uint16_t i = 0; i < lengthof(e->lighting->illuminations); i++, il++) {

			if (il->diffuse == 0.0)
				break;

			VectorMA(color, il->diffuse / il->light.radius, il->light.color, color);
		}

		ColorNormalize(color, color);
	}

	for (int32_t i = 0; i < 3; i++) {
		color[i] *= e->color[i];
	}

	if (e->effects & EF_BLEND)
		color[3] = Clamp(e->color[3], 0.0, 1.0);
	else
		color[3] = 1.0;

	R_Color(color);
}

/*
 * @brief Populates hardware light sources with illumination information.
 */
static void R_ApplyMeshModelLighting_default(const r_entity_t *e) {

	vec4_t position = { 0.0, 0.0, 0.0, 1.0 };
	vec4_t diffuse = { 0.0, 0.0, 0.0, 1.0 };

	uint16_t i;
	for (i = 0; i < MAX_ACTIVE_LIGHTS; i++) {

		const r_illumination_t *il = &e->lighting->illuminations[i];

		if (il->diffuse == 0.0)
			break;

		VectorCopy(il->light.origin, position);
		glLightfv(GL_LIGHT0 + i, GL_POSITION, position);

		VectorCopy(il->light.color, diffuse);
		glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, diffuse);

		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, il->light.radius);
	}

	if (i < MAX_ACTIVE_LIGHTS) // disable the next light as a stop
		glLightf(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, 0.0);
}

/*
 * @brief Sets renderer state for the specified entity.
 */
static void R_SetMeshState_default(const r_entity_t *e) {

	if (e->model->mesh->num_frames == 1) { // bind static arrays
		R_SetArrayState(e->model);
	} else { // or use the default arrays
		R_ResetArrayState();

		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, e->model->texcoords);

		R_InterpolateMeshModel(e);
	}

	if (!r_draw_wireframe->value) {

		r_mesh_state.material = e->skins[0] ?: e->model->mesh->material;

		R_BindTexture(r_mesh_state.material->diffuse->texnum);

		R_SetMeshColor_default(e);

		if (e->effects & EF_ALPHATEST)
			R_EnableAlphaTest(true);

		if (e->effects & EF_BLEND)
			R_EnableBlend(true);

		if ((e->effects & EF_NO_LIGHTING) == 0 && r_state.lighting_enabled) {
			R_UseMaterial(r_mesh_state.material);

			R_ApplyMeshModelLighting_default(e);
		}
	} else {
		R_Color(NULL);

		R_UseMaterial(NULL);
	}

	if (e->effects & EF_WEAPON)
		glDepthRange(0.0, 0.3);

	R_RotateForEntity(e);
}

/*
 * @brief Restores renderer state for the given entity.
 */
static void R_ResetMeshState_default(const r_entity_t *e) {

	R_RotateForEntity(NULL);

	if (e->effects & EF_WEAPON)
		glDepthRange(0.0, 1.0);

	if (e->effects & EF_BLEND)
		R_EnableBlend(false);

	if (e->effects & EF_ALPHATEST)
		R_EnableAlphaTest(false);

	if (e->model->mesh->num_frames > 1) {
		if (texunit_diffuse.enabled) {
			R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
		}
	}
}

/*
 * @brief Draw the diffuse pass of each mesh segment for the specified model.
 */
static void R_DrawMeshParts_default(const r_entity_t *e, const r_md3_t *md3) {
	uint32_t offset = 0;

	const r_md3_mesh_t *mesh = md3->meshes;
	for (uint16_t i = 0; i < md3->num_meshes; i++, mesh++) {

		if (!r_draw_wireframe->value) {
			if (i > 0) { // update the diffuse state for the current mesh
				r_mesh_state.material = e->skins[i] ?: e->model->mesh->material;

				R_BindTexture(r_mesh_state.material->diffuse->texnum);

				R_UseMaterial(r_mesh_state.material);
			}
		}

		glDrawArrays(GL_TRIANGLES, offset, mesh->num_tris * 3);

		R_DrawMeshMaterial(r_mesh_state.material, offset, mesh->num_tris * 3);

		offset += mesh->num_tris * 3;
	}
}

/*
 * @brief Draws the mesh model for the given entity.
 */
void R_DrawMeshModel_default(const r_entity_t *e) {

	R_SetMeshState_default(e);

	if (e->model->type == MOD_MD3) {
		R_DrawMeshParts_default(e, (const r_md3_t *) e->model->mesh->data);
	} else {
		glDrawArrays(GL_TRIANGLES, 0, e->model->num_verts);

		R_DrawMeshMaterial(r_mesh_state.material, 0, e->model->num_verts);
	}

	R_ResetMeshState_default(e); // reset state

	r_view.num_mesh_models++;
	r_view.num_mesh_tris += e->model->num_verts / 3;
}

/*
 * @brief Draws all mesh models for the current frame.
 */
void R_DrawMeshModels_default(const r_entities_t *ents) {

	R_EnableLighting(r_state.default_program, true);

	if (r_draw_wireframe->value) {
		R_EnableTexture(&texunit_diffuse, false);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	for (size_t i = 0; i < ents->count; i++) {
		const r_entity_t *e = ents->entities[i];

		if (e->effects & EF_NO_DRAW)
			continue;

		r_view.current_entity = e;

		R_DrawMeshModel_default(e);
	}

	r_view.current_entity = NULL;

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		R_EnableTexture(&texunit_diffuse, true);
	}

	R_EnableLighting(NULL, false);

	R_EnableFog(false);

	R_EnableBlend(true);

	R_DrawMeshShadows_default(ents);

	R_DrawMeshShells_default(ents);

	R_EnableBlend(false);

	R_EnableFog(true);

}
