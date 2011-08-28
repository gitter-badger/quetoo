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

#include "renderer.h"

// entities are chained together by type to reduce state changes
typedef struct r_entities_s {
	r_entity_t *bsp;
	r_entity_t *mesh;
	r_entity_t *mesh_alpha_test;
	r_entity_t *mesh_blend;
	r_entity_t *null;
} r_entities_t;

r_entities_t r_entities;


/*
 * R_EntityList
 *
 * Returns the appropriate entity list for the specified entity.
 */
static r_entity_t **R_EntityList(const r_entity_t *e) {

	if(!e->model)
		return &r_entities.null;

	if(e->model->type == mod_bsp_submodel)
		return &r_entities.bsp;

	// mesh models

	if(e->effects & EF_ALPHATEST)
		return &r_entities.mesh_alpha_test;

	if(e->effects & EF_BLEND)
		return &r_entities.mesh_blend;

	return &r_entities.mesh;
}


/*
 * R_CullEntity
 *
 * Returns true if the specified entity is outside of the view frustum, false
 * otherwise.
 */
static qboolean R_CullEntity(r_entity_t *e){

	if(!e->model)  // don't bother culling null models
		return false;

	if(e->model->type == mod_bsp_submodel)
		return R_CullBspModel(e);

	// for mesh models, apply translation and scale before culling

	R_ApplyMeshModelConfig(e);

	return R_CullMeshModel(e);
}


/*
 * R_AddEntity
 *
 * Adds a copy of the specified entity to the correct draw list.  Entities
 * are grouped by model to allow instancing wherever possible (e.g. armor
 * shards, ammo boxes, trees, etc..).
 */
r_entity_t *R_AddEntity(const r_entity_t *ent){
	r_entity_t *e, *in, **ents;

	if(r_view.num_entities == MAX_ENTITIES){
		Com_Warn("R_AddEntity: MAX_ENTITIES reached.\n");
		return NULL;
	}

	e = &r_view.entities[r_view.num_entities++];
	*e = *ent;  // copy in to renderer array

	if(R_CullEntity(e)){  // cull it, discarding entities which fail
		r_view.num_entities--;
		return NULL;
	}

	// setup the transform matrix
	if(!(e->effects & EF_LINKED)){
		Matrix4x4_CreateFromQuakeEntity(&e->matrix,
				e->origin[0], e->origin[1], e->origin[2],
				e->angles[0], e->angles[1], e->angles[2],
				e->scale[0]);
	}

	// and insert into the sorted list
	ents = R_EntityList(e);
	in = *ents;

	while(in){

		if(in->model == e->model){
			e->next = in->next;
			in->next = e;
			return e;
		}

		in = in->next;
	}

	// or simply push to the head of the chain
	e->next = *ents;
	*ents = e;
	return e;
}


/*
 * R_RotateForEntity
 *
 * Applies translation, rotation, and scale for the specified entity.
 */
void R_RotateForEntity(const r_entity_t *e){

	if(!e){
		glPopMatrix();
		return;
	}

	GLfloat mat[16];

	Matrix4x4_ToArrayFloatGL(&e->matrix, mat);

	glPushMatrix();

	glMultMatrixf(mat);
}


/*
 *
 * R_TransformForEntity
 *
 * Transforms a point by the inverse of the world-model matrix for the
 * specified entity.
 */
void R_TransformForEntity(const r_entity_t *e, const vec3_t in, vec3_t out){
	matrix4x4_t mat;

	Matrix4x4_Invert_Simple(&mat, &e->matrix);

	Matrix4x4_Transform(&mat, in, out);
}


/*
 * R_DrawBspEntities
 */
static void R_DrawBspEntities(){
	const r_entity_t *e;

	e = r_entities.bsp;

	while(e){
		R_DrawBspModel(e);
		e = e->next;
	}
}


/*
 * R_DrawMeshEntities
 */
static void R_DrawMeshEntities(r_entity_t *ents){
	r_entity_t *e;

	e = ents;

	while(e){
		R_DrawMeshModel(e);
		e = e->next;
	}
}


/*
 * R_DrawOpaqueMeshEntities
 */
static void R_DrawOpaqueMeshEntities(){

	if(!r_entities.mesh)
		return;

	R_EnableLighting(r_state.mesh_program, true);

	R_DrawMeshEntities(r_entities.mesh);

	R_EnableLighting(NULL, false);
}


/*
 * R_DrawAlphaTestMeshEntities
 */
static void R_DrawAlphaTestMeshEntities(){

	if(!r_entities.mesh_alpha_test)
		return;

	R_EnableAlphaTest(true);

	R_EnableLighting(r_state.mesh_program, true);

	R_DrawMeshEntities(r_entities.mesh_alpha_test);

	R_EnableLighting(NULL, false);

	R_EnableAlphaTest(false);
}


/*
 * R_DrawBlendMeshEntities
 */
static void R_DrawBlendMeshEntities(){

	if(!r_entities.mesh_blend)
		return;

	R_EnableBlend(true);

	R_DrawMeshEntities(r_entities.mesh_blend);

	R_EnableBlend(false);
}


/*
 * R_DrawNullModel
 *
 * Draws a place-holder "white diamond" prism for the specified entity.
 */
static void R_DrawNullModel(const r_entity_t *e){
	int i;

	R_EnableTexture(&texunit_diffuse, false);

	R_RotateForEntity(e);

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0.0, 0.0, -16.0);
	for(i = 0; i <= 4; i++)
		glVertex3f(16.0 * cos(i * M_PI / 2.0), 16.0 * sin(i * M_PI / 2.0), 0.0);
	glEnd();

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0.0, 0.0, 16.0);
	for(i = 4; i >= 0; i--)
		glVertex3f(16.0 * cos(i * M_PI / 2.0), 16.0 * sin(i * M_PI / 2.0), 0.0);
	glEnd();

	R_RotateForEntity(NULL);

	R_EnableTexture(&texunit_diffuse, true);
}


/*
 * R_DrawNullEntities
 */
static void R_DrawNullEntities(){
	const r_entity_t *e;

	if(!r_entities.null)
		return;

	e = r_entities.null;

	while(e){
		R_DrawNullModel(e);
		e = e->next;
	}
}


/*
 * R_DrawEntities
 *
 * Primary entry point for drawing all entities.
 */
void R_DrawEntities(void){

	if(r_draw_wireframe->value){
		R_BindTexture(r_null_image->texnum);
		R_EnableTexture(&texunit_diffuse, false);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	R_DrawOpaqueMeshEntities();

	R_DrawAlphaTestMeshEntities();

	R_DrawBlendMeshEntities();

	if(r_draw_wireframe->value){
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		R_EnableTexture(&texunit_diffuse, true);
	}

	glColor4ubv(color_white);

	R_DrawBspEntities();

	R_DrawNullEntities();

	// clear draw lists for next frame
	memset(&r_entities, 0, sizeof(r_entities));
}
