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

#include "cvar.h"
#include "cmd.h"
#include "common.h"
#include "console.h"
#include "cmodel.h"
#include "game/game.h"
#include "net.h"
#include "pmove.h"
#include "sys.h"

typedef enum {
	ss_dead,   // no level loaded
	ss_loading,   // spawning level edicts
	ss_game,   // actively running
	ss_demo
} sv_state_t;

typedef struct sv_server_s {
	sv_state_t state;  // precache commands are only valid during load

	unsigned time;  // always sv.framenum * 100 msec
	int framenum;

	char name[MAX_QPATH];  // map name
	struct cmodel_s	*models[MAX_MODELS];

	char configstrings[MAX_CONFIGSTRINGS][MAX_STRING_CHARS];
	entity_state_t baselines[MAX_EDICTS];

	// the multicast buffer is used to send a message to a set of clients
	// it is only used to marshall data until Sv_Multicast is called
	sizebuf_t multicast;
	byte multicast_buf[MAX_MSGLEN];

	// demo server information
	FILE *demofile;
} sv_server_t;

extern sv_server_t sv;  // local server

typedef enum {
	cs_free,   // can be used for a new connection
	cs_connected,   // client is connecting, but has not yet spawned
	cs_spawned  // client is spawned
} sv_client_state_t;

typedef struct sv_frame_s {
	int areabytes;
	byte areabits[MAX_BSP_AREAS / 8];  // portal area visibility bits
	player_state_t ps;
	int num_entities;
	int first_entity;  // into the circular sv_packet_entities[]
	int senttime;  // for ping calculations
} sv_frame_t;

#define LATENCY_COUNTS 16
#define RATE_MESSAGES 10

#define MSEC_OKAY_MIN 20  // at least 20ms for normal movement
#define MSEC_OKAY_MAX 1800 // 1600 for valid movement, plus some slop
#define MSEC_OKAY_STEP -1  // decrement count for normal movements
#define MSEC_ERROR_MAX "30"  // three sequential bad moves probably means cheater
#define MSEC_ERROR_STEP 10  // increment for bad movements

typedef struct sv_client_s {
	sv_client_state_t state;

	char userinfo[MAX_INFO_STRING];  // name, etc

	int lastframe;  // for delta compression
	usercmd_t lastcmd;  // for filling in big drops

	int cmd_msec;  // every seconds this is reset, if user
	// commands exhaust it, assume time cheating
	int cmd_msec_errors;  // maintain how many msec problems we've seen

	int frame_latency[LATENCY_COUNTS];
	int ping;

	int message_size[RATE_MESSAGES];  // used to rate drop packets
	int rate;
	int surpress_count;  // number of messages rate supressed

	edict_t *edict;  // EDICT_NUM(clientnum+1)
	char name[32];  // extracted from userinfo, high bits masked
	int messagelevel;  // for filtering printed messages

	// The datagram is written to by sound calls, prints, temp ents, etc.
	// It can be harmlessly overflowed.
	sizebuf_t datagram;
	byte datagram_buf[MAX_MSGLEN];

	sv_frame_t frames[UPDATE_BACKUP];  // updates can be delta'd from here

	byte *download;  // file being downloaded
	int downloadsize;  // total bytes (can't use EOF because of paks)
	int downloadcount;  // bytes sent

	int lastmessage;  // sv.framenum when packet was last received
	int lastconnect;

	int challenge;  // challenge of this user, randomly generated

	qboolean recording;  // client is currently recording a demo

	int extensions;  // bitmapped enhanced capabilities

	netchan_t netchan;
} sv_client_t;

// the server runs fixed-interval frames at a configurable rate (Hz)
#define MIN_PACKETRATE 10
#define MAX_PACKETRATE 120

#define MAX_MASTERS	8  // max recipients for heartbeat packets

// MAX_CHALLENGES is made large to prevent a denial
// of service attack that could cycle all of them
// out before legitimate users connected
#define MAX_CHALLENGES	1024

typedef struct sv_challenge_s {
	netaddr_t addr;
	int challenge;
	int time;
} sv_challenge_t;

typedef struct sv_static_s {
	qboolean initialized;  // sv_init has completed
	int realtime;  // always increasing, no clamping, etc

	int spawncount;  // incremented each level start, used to check late spawns

	int packetrate;  // packets per second to clients

	sv_client_t *clients;  // [maxclients->value];
	int num_client_entities;  // maxclients->value * UPDATE_BACKUP * MAX_PACKET_ENTITIES
	int next_client_entities;  // next client_entity to use
	entity_state_t *client_entities;  // [num_client_entities]

	netaddr_t masters[MAX_MASTERS];
	int last_heartbeat;

	sv_challenge_t challenges[MAX_CHALLENGES];  // to prevent invalid IPs from connecting
} sv_static_t;

extern sv_static_t svs;  // persistent server info

// macros for resolving game entities on the server
#define EDICT_FOR_NUM(n)( (edict_t *)((void *)ge->edicts + ge->edict_size*(n)) )
#define NUM_FOR_EDICT(e)( ((void *)(e) - (void *)ge->edicts) / ge->edict_size )
#define EDICT_FOR_CLIENT(c)(EDICT_FOR_NUM(c - svs.clients) + 1)

// cvars
extern cvar_t *sv_rcon_password;
extern cvar_t *sv_downloadurl;
extern cvar_t *sv_enforcetime;
extern cvar_t *sv_extensions;
extern cvar_t *sv_hostname;
extern cvar_t *sv_maxclients;
extern cvar_t *sv_maxrate;
extern cvar_t *sv_packetrate;
extern cvar_t *sv_public;
extern cvar_t *sv_timeout;
extern cvar_t *sv_udpdownload;

// current client / player edict
extern sv_client_t *sv_client;
extern edict_t *sv_player;

// sv_main.c
void Sv_Init(void);
void Sv_Shutdown(const char *finalmsg, qboolean reconnect);
void Sv_Frame(int msec);
char *Sv_NetaddrToString(sv_client_t *cl);
void Sv_KickClient(sv_client_t *cl, const char *msg);
void Sv_DropClient(sv_client_t *cl);
void Sv_HeartbeatMasters(void);
void Sv_UserinfoChanged(sv_client_t *cl);

// sv_init.c
int Sv_ModelIndex(const char *name);
int Sv_SoundIndex(const char *name);
int Sv_ImageIndex(const char *name);
void Sv_Map(const char *levelstring);

// sv_send.c
typedef enum {
	RD_NONE,
	RD_CLIENT,
	RD_PACKET
} sv_redirect_t;

#define SV_OUTPUTBUF_LENGTH	(MAX_MSGLEN - 16)

extern char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

void Sv_FlushRedirect(int target, char *outputbuf);
void Sv_SendClientMessages(void);
void Sv_Unicast(edict_t *ent, qboolean reliable);
void Sv_Multicast(vec3_t origin, multicast_t to);
void Sv_PositionedSound(vec3_t origin, edict_t *entity, int soundindex, int atten);
void Sv_ClientPrint(edict_t *ent, int level, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Sv_ClientCenterPrint(edict_t *ent, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void Sv_BroadcastPrint(int level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void Sv_BroadcastCommand(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// sv_user.c
void Sv_ExecuteClientMessage(sv_client_t *cl);

// sv_ccmds.c
void Sv_InitOperatorCommands(void);

// sv_ents.c
void Sv_WriteFrameToClient(sv_client_t *client, sizebuf_t *msg);
void Sv_BuildClientFrame(sv_client_t *client);

// sv_game.c
extern game_export_t *ge;

void Sv_InitGameProgs(void);
void Sv_ShutdownGameProgs(void);

// sv_world.c
void Sv_ClearWorld(void);
// called after the world model has been loaded, before linking any entities

void Sv_UnlinkEdict(edict_t *ent);
// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself

void Sv_LinkEdict(edict_t *ent);
// Needs to be called any time an entity changes origin, mins, maxs,
// or solid.  Automatically unlinks if needed.
// sets ent->v.absmin and ent->v.absmax
// sets ent->leafnums[] for pvs determination even if the entity
// is not solid

int Sv_AreaEdicts(vec3_t mins, vec3_t maxs, edict_t **list, int maxcount, int areatype);
// fills in a table of edict pointers with edicts that have bounding boxes
// that intersect the given area.  It is possible for a non-axial bmodel
// to be returned that doesn't actually intersect the area on an exact
// test.
// returns the number of pointers filled in
// ??? does this always return the world?


// functions that interact with everything apropriate
int Sv_PointContents(vec3_t p);
// returns the CONTENTS_* value from the world at the given point.
// Quake 2 extends this to also check entities, to allow moving liquids


trace_t Sv_Trace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passedict, int contentmask);
// mins and maxs are relative

// if the entire move stays in a solid volume, trace.allsolid will be set,
// trace.startsolid will be set, and trace.fraction will be 0

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// passedict is explicitly excluded from clipping checks(normally NULL)

