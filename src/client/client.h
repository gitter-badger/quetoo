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

#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "cl_keys.h"
#include "cmodel.h"
#include "console.h"
#include "filesystem.h"
#include "mem.h"
#include "net.h"
#include "sys.h"
#include "renderer/renderer.h"
#include "sound/sound.h"

typedef struct cl_frame_s {
	qboolean valid;  // cleared if delta parsing was invalid
	int server_frame;
	int server_time;  // server time the message is valid for (in milliseconds)
	int delta_frame;
	byte area_bits[MAX_BSP_AREAS / 8];  // portal area visibility bits
	player_state_t ps;
	int num_entities;
	int entity_state;  // non-masked index into cl.entity_states array
} cl_frame_t;

# define MAX_ENTITY_LIGHTING 4  // the number of static lighting caches each entity might use

typedef struct cl_entity_s {
	entity_state_t baseline;  // delta from this if not from a previous frame
	entity_state_t current;
	entity_state_t prev;  // will always be valid, but might just be a copy of current

	int server_frame;  // if not current, this entity isn't in the frame

	int time;  // for intermittent effects

	int anim_time;  // for animations
	int anim_frame;

	r_lighting_t lighting[MAX_ENTITY_LIGHTING];  // cached static lighting info
} cl_entity_t;

#define MAX_WEAPON_MODELS 12

typedef struct client_info_s {
	char name[MAX_QPATH];
	char cinfo[MAX_QPATH];
	struct r_image_s *skin;
	struct r_model_s *model;
	struct r_model_s *weapon_model[MAX_WEAPON_MODELS];
} cl_client_info_t;

#define CMD_BACKUP 128  // allow a lot of command backups for very fast systems
#define CMD_MASK (CMD_BACKUP - 1)

// we accumulate parsed entity states in a rather large buffer so that they
// may be safely delta'd in the future
#define ENTITY_STATE_BACKUP (UPDATE_BACKUP * MAX_PACKET_ENTITIES)
#define ENTITY_STATE_MASK (ENTITY_STATE_BACKUP - 1)

// the cl_client_s structure is wiped completely at every map change
typedef struct cl_client_s {
	int timedemo_frames;
	int timedemo_start;

	user_cmd_t cmds[CMD_BACKUP];  // each message will send several old cmds
	int cmd_time[CMD_BACKUP];  // time sent, for calculating pings

	vec_t predicted_step;
	int predicted_step_time;

	vec3_t predicted_origin;  // generated by Cl_PredictMovement
	vec3_t predicted_angles;
	vec3_t prediction_error;
	short predicted_origins[CMD_BACKUP][3];  // for debug comparing against server

	qboolean underwater;  // updated by client sided prediction

	cl_frame_t frame;  // received from server
	cl_frame_t frames[UPDATE_BACKUP];  // for calculating delta compression

	cl_entity_t entities[MAX_EDICTS];  // client entities

	entity_state_t entity_states[ENTITY_STATE_BACKUP];  // accumulated each frame
	int entity_state;  // index (not wrapped) into entity states

	int player_num;  // our entity number

	int surpress_count;  // number of messages rate suppressed

	int time;  // this is the server time value that the client
	// is rendering at.  always <= cls.real_time due to latency

	float lerp;  // linear interpolation between frames

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta when necessary which is added to the locally
	// tracked view angles to account for spawn and teleport direction changes
	vec3_t angles;

	char layout[MAX_STRING_CHARS];  // general 2D overlay

	int server_count;  // server identification for precache
	int server_frame_rate;  // server frame rate (packets per second)

	qboolean demo_server;  // we're viewing a demo

	char gamedir[MAX_QPATH];
	char config_strings[MAX_CONFIG_STRINGS][MAX_STRING_CHARS];

	// locally derived information from server state
	r_model_t *model_draw[MAX_MODELS];
	c_model_t *model_clip[MAX_MODELS];

	s_sample_t *sound_precache[MAX_SOUNDS];
	r_image_t *image_precache[MAX_IMAGES];

	char weapon_models[MAX_WEAPON_MODELS][MAX_QPATH];
	int num_weapon_models;

	cl_client_info_t client_info[MAX_CLIENTS];
	cl_client_info_t base_client_info;
} cl_client_t;

extern cl_client_t cl;

// the client_static_t structure is persistent through an arbitrary
// number of server connections

typedef enum {
	ca_uninitialized,
	ca_disconnected,    // not talking to a server
	ca_connecting,   // sending request packets to the server
	ca_connected,   // netchan_t established, waiting for svc_server_data
	ca_active  // game views should be displayed
} cl_state_t;

typedef enum {
	key_game,
	key_menu,
	key_console,
	key_message
} cl_key_dest_t;

typedef struct cl_key_state_s {
	cl_key_dest_t dest;

	char lines[KEY_HISTORYSIZE][KEY_LINESIZE];
	int pos;

	qboolean insert;

	unsigned edit_line;
	unsigned history_line;

	char *binds[K_LAST];
	qboolean down[K_LAST];
} cl_key_state_t;

typedef struct cl_mouse_state_s {
	float x, y;
	float old_x, old_y;
	qboolean grabbed;
} cl_mouse_state_t;

typedef struct cl_chat_state_s {
	char buffer[KEY_LINESIZE];
	size_t len;
	qboolean team;
} cl_chat_state_t;

typedef struct cl_download_s {
	qboolean http;
	FILE *file;
	char tempname[MAX_OSPATH];
	char name[MAX_OSPATH];
} cl_download_t;

typedef enum {
	SERVER_SOURCE_INTERNET,
	SERVER_SOURCE_USER,
	SERVER_SOURCE_BCAST
} cl_server_source_t;

typedef struct cl_server_info_s {
	net_addr_t addr;
	cl_server_source_t source;
	int pingtime;
	int ping;
	char info[MAX_MSG_SIZE];
	int num;
	struct cl_server_info_s *next;
} cl_server_info_t;

#define MAX_SERVER_INFOS 128

typedef struct cl_static_s {
	cl_state_t state;

	cl_key_state_t key_state;

	cl_mouse_state_t mouse_state;

	cl_chat_state_t chat_state;

	int real_time;  // always increasing, no clamping, etc

	int packet_delta;  // millis since last outgoing packet
	int render_delta;  // millis since last renderer frame

	// connection information
	char server_name[MAX_OSPATH];  // name of server to connect to
	float connect_time;  // for connection retransmits

	net_chan_t netchan;  // network channel

	int challenge;  // from the server to use for connecting

	int loading;  // loading percentage indicator

	char download_url[MAX_OSPATH];  // for http downloads
	cl_download_t download;  // current download (udp or http)

	qboolean demo_waiting;  // don't begin recording until an uncompressed message is received
	char demo_path[MAX_OSPATH];
	FILE *demo_file;

	cl_server_info_t *servers;  // list of servers from all sources
	char *servers_text;  // tabular data for servers menu

	int broadcast_time;  // time when last broadcast ping was sent
} cl_static_t;

extern cl_static_t cls;

// cvars
extern cvar_t *cl_add_entities;
extern cvar_t *cl_add_particles;
extern cvar_t *cl_async;
extern cvar_t *cl_blend;
extern cvar_t *cl_bob;
extern cvar_t *cl_chat_sound;
extern cvar_t *cl_counters;
extern cvar_t *cl_crosshair;
extern cvar_t *cl_crosshair_color;
extern cvar_t *cl_crosshair_scale;
extern cvar_t *cl_emits;
extern cvar_t *cl_fov;
extern cvar_t *cl_fov_zoom;
extern cvar_t *cl_hud;
extern cvar_t *cl_ignore;
extern cvar_t *cl_max_fps;
extern cvar_t *cl_max_pps;
extern cvar_t *cl_net_graph;
extern cvar_t *cl_predict;
extern cvar_t *cl_show_prediction_misses;
extern cvar_t *cl_show_net_messages;
extern cvar_t *cl_team_chat_sound;
extern cvar_t *cl_third_person;
extern cvar_t *cl_timeout;
extern cvar_t *cl_view_size;
extern cvar_t *cl_weapon;
extern cvar_t *cl_weather;

extern cvar_t *rcon_password;
extern cvar_t *rcon_address;

// user_info
extern cvar_t *color;
extern cvar_t *message_level;
extern cvar_t *name;
extern cvar_t *password;
extern cvar_t *rate;
extern cvar_t *skin;

extern cvar_t *recording;

// cl_screen.c
void Cl_CenterPrint(char *s);
void Cl_AddNetgraph(void);
void Cl_UpdateScreen(void);

// cl_emit.c
void Cl_LoadEmits(void);
void Cl_AddEmits(void);

// cl_entity.c
unsigned int Cl_ParseEntityBits(unsigned int *bits);
void Cl_ParseDelta(const entity_state_t *from, entity_state_t *to, int number, int bits);
void Cl_ParseFrame(void);
void Cl_AddEntities(cl_frame_t *frame);

// cl_tentity.c
void Cl_LoadTempEntitySamples(void);
void Cl_ParseTempEntity(void);

// cl_main.c
void Cl_Init(void);
void Cl_Frame(int msec);
void Cl_Shutdown(void);
void Cl_SendDisconnect(void);
void Cl_Disconnect(void);
void Cl_Reconnect_f(void);
void Cl_LoadProgress(int percent);
void Cl_RequestNextDownload(void);
void Cl_WriteDemoMessage(void);

// cl_input.c
void Cl_InitInput(void);
void Cl_HandleEvents(void);
void Cl_Move(user_cmd_t *cmd);

// cl_cmd.c
void Cl_UpdateCmd(void);
void Cl_SendCmd(void);

// cl_console.c
extern console_t cl_con;

void Cl_InitConsole(void);
void Cl_DrawConsole(void);
void Cl_DrawNotify(void);
void Cl_UpdateNotify(int lastline);
void Cl_ClearNotify(void);
void Cl_ToggleConsole_f(void);

// cl_keys.c
void Cl_KeyEvent(unsigned int ascii, unsigned short unicode, qboolean down, unsigned time);
char *Cl_EditLine(void);
void Cl_WriteBindings(FILE *f);
void Cl_InitKeys(void);
void Cl_ShutdownKeys(void);
void Cl_ClearTyping(void);

// cl_parse.c
extern char *svc_strings[256];

qboolean Cl_CheckOrDownloadFile(const char *file_name);
void Cl_ParseConfigString(void);
void Cl_ParseClientInfo(int player);
void Cl_ParseMuzzleFlash(void);
void Cl_ParseServerMessage(void);
void Cl_LoadClientInfo(cl_client_info_t *ci, const char *s);
void Cl_Download_f(void);

// cl_view.c
void Cl_InitView(void);
void Cl_ClearState(void);
void Cl_AddEntity(r_entity_t *ent);
void Cl_AddParticle(r_particle_t *p);
void Cl_UpdateView(void);

// cl_pred.c
extern int cl_gravity;
void Cl_PredictMovement(void);
void Cl_CheckPredictionError(void);

// cl_effect.c
void Cl_LightningEffect(const vec3_t org);
void Cl_LightningTrail(const vec3_t start, const vec3_t end);
void Cl_EnergyTrail(cl_entity_t *ent, float radius, int color);
void Cl_EnergyFlash(entity_state_t *ent, int color, int count);
void Cl_BFGEffect(const vec3_t org);
void Cl_BubbleTrail(const vec3_t start, const vec3_t end, float density);
void Cl_EntityEvent(entity_state_t *ent);
void Cl_BulletTrail(const vec3_t start, const vec3_t end);
void Cl_BulletEffect(const vec3_t org, const vec3_t dir);
void Cl_BurnEffect(const vec3_t org, const vec3_t dir, int scale);
void Cl_BloodEffect(const vec3_t org, const vec3_t dir, int count);
void Cl_GibEffect(const vec3_t org, int count);
void Cl_SparksEffect(const vec3_t org, const vec3_t dir, int count);
void Cl_RailTrail(const vec3_t start, const vec3_t end, int flags, int color);
void Cl_SmokeTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent);
void Cl_SmokeFlash(entity_state_t *ent);
void Cl_FlameTrail(const vec3_t start, const vec3_t end, cl_entity_t *ent);
void Cl_SteamTrail(const vec3_t org, const vec3_t vel, cl_entity_t *ent);
void Cl_ExplosionEffect(const vec3_t org);
void Cl_ItemRespawnEffect(const vec3_t org);
void Cl_ItemPickupEffect(const vec3_t org);
void Cl_TeleporterTrail(const vec3_t org, cl_entity_t *ent);
void Cl_LogoutEffect(const vec3_t org);
void Cl_LoadEffectSamples(void);
void Cl_AddParticles(void);
r_particle_t *Cl_AllocParticle(void);
void Cl_ClearEffects(void);

// cl_http.c
void Cl_InitHttpDownload(void);
void Cl_HttpDownloadCleanup(void);
qboolean Cl_HttpDownload(void);
void Cl_HttpDownloadThink(void);
void Cl_ShutdownHttpDownload(void);

//cl_loc.c
void Cl_InitLocations(void);
void Cl_ShutdownLocations(void);
void Cl_LoadLocations(void);
const char *Cl_LocationHere(void);
const char *Cl_LocationThere(void);

// cl_server.c
void Cl_Ping_f(void);
void Cl_Servers_f(void);
void Cl_ParseStatusMessage(void);
void Cl_ParseServersList(void);
void Cl_FreeServers(void);
cl_server_info_t *Cl_ServerForNum(int num);

#endif /* __CLIENT_H__ */
