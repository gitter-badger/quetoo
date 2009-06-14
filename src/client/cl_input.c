/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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

#include <SDL.h>

#include "client.h"
#include "keys.h"

static cvar_t *cl_run;

static cvar_t *m_sensitivity;
static cvar_t *m_sensitivityzoom;
static cvar_t *m_interpolate;
static cvar_t *m_invert;
static cvar_t *m_yaw;
static cvar_t *m_pitch;

/* power of two please */
#define MAX_KEYQ 64

struct {
	unsigned int key;
	unsigned short unicode;
	int down;
	int repeat;
} keyq[MAX_KEYQ];

static int keyq_head = 0;
static int keyq_tail = 0;

#define EVENT_ENQUEUE(keyNum, keyUnicode, keyDown) \
	if(keyNum > 0){ \
		keyq[keyq_head].key = (keyNum); \
		keyq[keyq_head].unicode = (keyUnicode); \
		keyq[keyq_head].down = (keyDown); \
		keyq_head = (keyq_head + 1) & (MAX_KEYQ - 1); \
	}

// mouse vars
static qboolean mouse_active;
static float mouse_x, mouse_y;
static float old_mouse_x, old_mouse_y;


/*
 * KEY BUTTONS
 *
 * Continuous button event tracking is complicated by the fact that two different
 * input sources (say, mouse button 1 and the control key) can both press the
 * same button, but the button should only be released when both of the
 * pressing key have been released.
 *
 * When a key event issues a button command (+forward, +attack, etc), it appends
 * its key number as a parameter to the command so it can be matched up with
 * the release.
 *
 * state bit 0 is the current state of the key
 * state bit 1 is edge triggered on the up to down transition
 * state bit 2 is edge triggered on the down to up transition
 */

typedef struct {
	int down[2];  // key nums holding it down
	unsigned downtime;  // msec timestamp
	unsigned msec;  // msec down this frame
	int state;
} kbutton_t;

static kbutton_t in_left, in_right, in_forward, in_back;
static kbutton_t in_lookup, in_lookdown;
static kbutton_t in_moveleft, in_moveright;
static kbutton_t in_speed, in_attack;
static kbutton_t in_up, in_down;


/*
 * Cl_KeyDown
 */
static void Cl_KeyDown(kbutton_t *b){
	int k;
	char *c;

	c = Cmd_Argv(1);
	if(c[0])
		k = atoi(c);
	else
		k = -1;  // typed manually at the console for continuous down

	if(k == b->down[0] || k == b->down[1])
		return;  // repeating key

	if(!b->down[0])
		b->down[0] = k;
	else if(!b->down[1])
		b->down[1] = k;
	else {
		Com_Printf("Three keys down for a button!\n");
		return;
	}

	if(b->state & 1)
		return;  // still down

	// save timestamp
	c = Cmd_Argv(2);
	b->downtime = atoi(c);
	if(!b->downtime)
		b->downtime = cls.realtime - 100;

	b->state |= 1;
}


/*
 * Cl_KeyUp
 */
static void Cl_KeyUp(kbutton_t *b){
	int k;
	char *c;
	unsigned uptime;

	c = Cmd_Argv(1);
	if(c[0])
		k = atoi(c);
	else { // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		return;
	}

	if(b->down[0] == k)
		b->down[0] = 0;
	else if(b->down[1] == k)
		b->down[1] = 0;
	else
		return;  // key up without coresponding down

	if(b->down[0] || b->down[1])
		return;  // some other key is still holding it down

	if(!(b->state & 1))
		return;  // still up (this should not happen)

	// save timestamp
	c = Cmd_Argv(2);
	uptime = atoi(c);
	if(uptime)
		b->msec += uptime - b->downtime;
	else
		b->msec += 10;

	b->state &= ~1;  // now up
}


static void Cl_UpDown_f(void){
	Cl_KeyDown(&in_up);
}
static void Cl_UpUp_f(void){
	Cl_KeyUp(&in_up);
}
static void Cl_DownDown_f(void){
	Cl_KeyDown(&in_down);
}
static void Cl_DownUp_f(void){
	Cl_KeyUp(&in_down);
}
static void Cl_LeftDown_f(void){
	Cl_KeyDown(&in_left);
}
static void Cl_LeftUp_f(void){
	Cl_KeyUp(&in_left);
}
static void Cl_RightDown_f(void){
	Cl_KeyDown(&in_right);
}
static void Cl_RightUp_f(void){
	Cl_KeyUp(&in_right);
}
static void Cl_ForwardDown_f(void){
	Cl_KeyDown(&in_forward);
}
static void Cl_ForwardUp_f(void){
	Cl_KeyUp(&in_forward);
}
static void Cl_BackDown_f(void){
	Cl_KeyDown(&in_back);
}
static void Cl_BackUp_f(void){
	Cl_KeyUp(&in_back);
}
static void Cl_LookupDown_f(void){
	Cl_KeyDown(&in_lookup);
}
static void Cl_LookupUp_f(void){
	Cl_KeyUp(&in_lookup);
}
static void Cl_LookdownDown_f(void){
	Cl_KeyDown(&in_lookdown);
}
static void Cl_LookdownUp_f(void){
	Cl_KeyUp(&in_lookdown);
}
static void Cl_MoveleftDown_f(void){
	Cl_KeyDown(&in_moveleft);
}
static void Cl_MoveleftUp_f(void){
	Cl_KeyUp(&in_moveleft);
}
static void Cl_MoverightDown_f(void){
	Cl_KeyDown(&in_moveright);
}
static void Cl_MoverightUp_f(void){
	Cl_KeyUp(&in_moveright);
}
static void Cl_SpeedDown_f(void){
	Cl_KeyDown(&in_speed);
}
static void Cl_SpeedUp_f(void){
	Cl_KeyUp(&in_speed);
}
static void Cl_AttackDown_f(void){
	Cl_KeyDown(&in_attack);
}
static void Cl_AttackUp_f(void){
	Cl_KeyUp(&in_attack);
}
static void Cl_CenterView_f(void){
	cl.angles[PITCH] = 0;
}


/*
 * Cl_KeyState
 *
 * Returns the fraction of the command interval for which the key was down.
 */
static float Cl_KeyState(kbutton_t *key, int cmd_msec){
	int msec;
	float v;

	msec = key->msec;
	key->msec = 0;

	if(key->state){  // still down, reset downtime for next frame
		msec += cls.realtime - key->downtime;
		key->downtime = cls.realtime;
	}

	v = (float)(msec / cmd_msec);

	if(v > 1.0)
		v = 1.0;
	else if(v < 0.0)
		v = 0.0;

	return v;
}


/*
 * Cl_KeyMap
 */
static void Cl_KeyMap(SDL_Event *event, unsigned int *ascii, unsigned short *unicode){
	int key = 0;
	const unsigned int keysym = event->key.keysym.sym;

	switch(keysym){
		case SDLK_KP9:
			key = K_KP_PGUP;
			break;
		case SDLK_PAGEUP:
			key = K_PGUP;
			break;

		case SDLK_KP3:
			key = K_KP_PGDN;
			break;
		case SDLK_PAGEDOWN:
			key = K_PGDN;
			break;

		case SDLK_KP7:
			key = K_KP_HOME;
			break;
		case SDLK_HOME:
			key = K_HOME;
			break;

		case SDLK_KP1:
			key = K_KP_END;
			break;
		case SDLK_END:
			key = K_END;
			break;

		case SDLK_KP4:
			key = K_KP_LEFTARROW;
			break;
		case SDLK_LEFT:
			key = K_LEFTARROW;
			break;

		case SDLK_KP6:
			key = K_KP_RIGHTARROW;
			break;
		case SDLK_RIGHT:
			key = K_RIGHTARROW;
			break;

		case SDLK_KP2:
			key = K_KP_DOWNARROW;
			break;
		case SDLK_DOWN:
			key = K_DOWNARROW;
			break;

		case SDLK_KP8:
			key = K_KP_UPARROW;
			break;
		case SDLK_UP:
			key = K_UPARROW;
			break;

		case SDLK_ESCAPE:
			key = K_ESCAPE;
			break;
		case SDLK_KP_ENTER:
			key = K_KP_ENTER;
			break;
		case SDLK_RETURN:
			key = K_ENTER;
			break;

		case SDLK_TAB:
			key = K_TAB;
			break;

		case SDLK_F1:
			if(event->type == SDL_KEYDOWN)
				Cbuf_AddText("yes\n");
			break;
		case SDLK_F2:
			if(event->type == SDL_KEYDOWN)
				Cbuf_AddText("no\n");
			break;

		case SDLK_F3:
			key = K_F3;
			break;
		case SDLK_F4:
			key = K_F4;
			break;
		case SDLK_F5:
			key = K_F5;
			break;
		case SDLK_F6:
			key = K_F6;
			break;
		case SDLK_F7:
			key = K_F7;
			break;
		case SDLK_F8:
			key = K_F8;
			break;
		case SDLK_F9:
			key = K_F9;
			break;

		case SDLK_F10:
			key = K_F10;
			break;

		case SDLK_F11:
			if(event->type == SDL_KEYDOWN){
				Cvar_Toggle("r_fullscreen");
				R_Restart_f();
			}

			break;

		case SDLK_F12:
			key = K_F12;
			break;

		case SDLK_BACKSPACE:
			key = K_BACKSPACE;
			break;

		case SDLK_KP_PERIOD:
			key = K_KP_DEL;
			break;
		case SDLK_DELETE:
			key = K_DEL;
			break;

		case SDLK_PAUSE:
			key = K_PAUSE;
			break;

		case SDLK_LSHIFT:
		case SDLK_RSHIFT:
			key = K_SHIFT;
			break;

		case SDLK_LCTRL:
		case SDLK_RCTRL:
			key = K_CTRL;
			break;

		case SDLK_LMETA:
		case SDLK_RMETA:
		case SDLK_LALT:
		case SDLK_RALT:
			key = K_ALT;
			break;

		case SDLK_KP5:
			key = K_KP_5;
			break;

		case SDLK_INSERT:
			key = K_INS;
			break;
		case SDLK_KP0:
			key = K_KP_INS;
			break;

		case SDLK_KP_MULTIPLY:
			key = '*';
			break;
		case SDLK_KP_PLUS:
			key = K_KP_PLUS;
			break;
		case SDLK_KP_MINUS:
			key = K_KP_MINUS;
			break;
		case SDLK_KP_DIVIDE:
			key = K_KP_SLASH;
			break;

		case SDLK_WORLD_7:
			key = '`';
			break;

		default:
			key = keysym;
	}

	// if unicode is empty, use ascii instead
	*unicode = event->key.keysym.unicode == 0 ? key : event->key.keysym.unicode;
	*ascii = key;
}


/*
 * Cl_HandleEvent
 */
static void Cl_HandleEvent(SDL_Event *event){
	unsigned int key;
	unsigned short unicode;

	switch(event->type){
		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEBUTTONDOWN:
			switch(event->button.button){
				case 1:
					key = K_MOUSE1;
					break;
				case 2:
					key = K_MOUSE3;
					break;
				case 3:
					key = K_MOUSE2;
					break;
				case 4:
					key = K_MWHEELUP;
					break;
				case 5:
					key = K_MWHEELDOWN;
					break;
				case 6:
					key = K_MOUSE4;
					break;
				case 7:
					key = K_MOUSE5;
					break;
				default:
					key = K_AUX1 + (event->button.button - 8) % 16;
					break;
			}
			EVENT_ENQUEUE(key, key, (event->type == SDL_MOUSEBUTTONDOWN))
			break;

		case SDL_KEYDOWN:
		case SDL_KEYUP:
			Cl_KeyMap(event, &key, &unicode);
			EVENT_ENQUEUE(key, unicode, (event->type == SDL_KEYDOWN))
			break;
		case SDL_QUIT:
			Cmd_ExecuteString("quit");
			break;
	}
}


/*
 * Cl_MouseMove
 */
static void Cl_MouseMove(int mx, int my){

	if(m_interpolate->value){
		mouse_x = (mx + old_mouse_x) * 0.5;
		mouse_y = (my + old_mouse_y) * 0.5;
	} else {
		mouse_x = mx;
		mouse_y = my;
	}

	old_mouse_x = mx;
	old_mouse_y = my;

	if(mouse_x || mouse_y){
		mouse_x *= m_sensitivity->value;
		mouse_y *= m_sensitivity->value;

		if(m_invert->value)  // invert mouse
			mouse_y = -mouse_y;

		// add horizontal and vertical movement
		cl.angles[YAW] -= m_yaw->value * mouse_x;
		cl.angles[PITCH] += m_pitch->value * mouse_y;
	}
}


/*
 * Cl_HandleEvents
 */
void Cl_HandleEvents(void){

	SDL_Event event;

	if(!SDL_WasInit(SDL_INIT_VIDEO))
		return;

	// handle key events
	while(SDL_PollEvent(&event))
		Cl_HandleEvent(&event);

	if(!r_view.ready || cls.key_dest == key_console){
		if(mouse_active){  // yield cursor to os
			SDL_WM_GrabInput(SDL_GRAB_OFF);

			if(!r_state.fullscreen)
				SDL_ShowCursor(true);

			mouse_active = false;
		}
	} else {
		if(!mouse_active){  // or take it back
			SDL_WM_GrabInput(SDL_GRAB_ON);
			SDL_ShowCursor(false);
			mouse_active = true;
		}
	}

	if(mouse_active){  // check for movement
		int mx, my;
		SDL_GetMouseState(&mx, &my);

		mx -= r_state.width / 2;  // normalize to center
		my -= r_state.height / 2;

		if(m_sensitivity->modified) {
			// invalid or unreasonable value - set back to sane value
			if (m_sensitivity->value < 0.1)
				m_sensitivity->value = 3.0f;
			else if (m_sensitivity->value > 20.0f)
				m_sensitivity->value = 3.0f;
			m_sensitivity->modified = false;
		}

		if(mx || my){  // mouse has moved
			Cl_MouseMove(mx, my);
			SDL_WarpMouse(r_state.width / 2, r_state.height / 2);
		}

	}

	while(keyq_head != keyq_tail){  // then check for keys
		Cl_KeyEvent(keyq[keyq_tail].key, keyq[keyq_tail].unicode, keyq[keyq_tail].down, cls.realtime);
		keyq_tail = (keyq_tail + 1) & (MAX_KEYQ - 1);
	}
}


/*
 * Cl_ClampPitch
 */
static void Cl_ClampPitch(void){
	float pitch;

	// add frame's delta angles to our local input movement
	// wraping where appropriate
	pitch = SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[PITCH]);

	if(pitch > 180)
		pitch -= 360;

	if(cl.angles[PITCH] + pitch < -360)
		cl.angles[PITCH] += 360;
	if(cl.angles[PITCH] + pitch > 360)
		cl.angles[PITCH] -= 360;

	if(cl.angles[PITCH] + pitch > 89)
		cl.angles[PITCH] = 89 - pitch;
	if(cl.angles[PITCH] + pitch < -89)
		cl.angles[PITCH] = -89 - pitch;
}


/*
 * Cl_Move
 */
void Cl_Move(usercmd_t *cmd){
	float mod;
	int i;

	if(cmd->msec < 1)
		cmd->msec = 1;

	mod = 500.0;

	// keyboard move forward / back
	cmd->forwardmove += mod * Cl_KeyState(&in_forward, cmd->msec);
	cmd->forwardmove -= mod * Cl_KeyState(&in_back, cmd->msec);

	// keyboard strafe left / right
	cmd->sidemove += mod * Cl_KeyState(&in_moveright, cmd->msec);
	cmd->sidemove -= mod * Cl_KeyState(&in_moveleft, cmd->msec);

	// keyboard jump / crouch
	cmd->upmove += mod * Cl_KeyState(&in_up, cmd->msec);
	cmd->upmove -= mod * Cl_KeyState(&in_down, cmd->msec);

	mod = 2.0;  // reduce sensitivity for turning

	// keyboard turn left / right
	cl.angles[YAW] -= mod * Cl_KeyState(&in_right, cmd->msec);
	cl.angles[YAW] += mod * Cl_KeyState(&in_left, cmd->msec);

	// keyboard look up / down
	cl.angles[PITCH] -= mod * Cl_KeyState(&in_lookup, cmd->msec);
	cl.angles[PITCH] += mod * Cl_KeyState(&in_lookdown, cmd->msec);

	Cl_ClampPitch();  // clamp, accounting for frame delta angles

	for(i = 0; i < 3; i++)  // pack the angles into the command
		cmd->angles[i] = ANGLE2SHORT(cl.angles[i]);

	// set any button hits that occured since last frame
	if(in_attack.state & 3)
		cmd->buttons |= BUTTON_ATTACK;

	in_attack.state &= ~2;

	if(cl_run->value){  // run by default, walk on speed toggle
		if(in_speed.state & 1)
			cmd->buttons |= BUTTON_WALK;
	}
	else {  // walk by default, run on speed toggle
		if(!(in_speed.state & 1))
			cmd->buttons |= BUTTON_WALK;
	}

	if(key_numdown && cls.key_dest == key_game)  // something pressed
		cmd->buttons |= BUTTON_ANY;
}


/*
 * Cl_InitInput
 */
void Cl_InitInput(void){
	Cmd_AddCommand("centerview", Cl_CenterView_f, NULL);
	Cmd_AddCommand("+moveup", Cl_UpDown_f, NULL);
	Cmd_AddCommand("-moveup", Cl_UpUp_f, NULL);
	Cmd_AddCommand("+movedown", Cl_DownDown_f, NULL);
	Cmd_AddCommand("-movedown", Cl_DownUp_f, NULL);
	Cmd_AddCommand("+left", Cl_LeftDown_f, NULL);
	Cmd_AddCommand("-left", Cl_LeftUp_f, NULL);
	Cmd_AddCommand("+right", Cl_RightDown_f, NULL);
	Cmd_AddCommand("-right", Cl_RightUp_f, NULL);
	Cmd_AddCommand("+forward", Cl_ForwardDown_f, NULL);
	Cmd_AddCommand("-forward", Cl_ForwardUp_f, NULL);
	Cmd_AddCommand("+back", Cl_BackDown_f, NULL);
	Cmd_AddCommand("-back", Cl_BackUp_f, NULL);
	Cmd_AddCommand("+lookup", Cl_LookupDown_f, NULL);
	Cmd_AddCommand("-lookup", Cl_LookupUp_f, NULL);
	Cmd_AddCommand("+lookdown", Cl_LookdownDown_f, NULL);
	Cmd_AddCommand("-lookdown", Cl_LookdownUp_f, NULL);
	Cmd_AddCommand("+moveleft", Cl_MoveleftDown_f, NULL);
	Cmd_AddCommand("-moveleft", Cl_MoveleftUp_f, NULL);
	Cmd_AddCommand("+moveright", Cl_MoverightDown_f, NULL);
	Cmd_AddCommand("-moveright", Cl_MoverightUp_f, NULL);
	Cmd_AddCommand("+speed", Cl_SpeedDown_f, NULL);
	Cmd_AddCommand("-speed", Cl_SpeedUp_f, NULL);
	Cmd_AddCommand("+attack", Cl_AttackDown_f, NULL);
	Cmd_AddCommand("-attack", Cl_AttackUp_f, NULL);

	cl_run = Cvar_Get("cl_run", "1", CVAR_ARCHIVE, NULL);

	m_sensitivity = Cvar_Get("m_sensitivity", "3", CVAR_ARCHIVE, NULL);
	m_sensitivityzoom = Cvar_Get("m_sensitivityzoom", "1", CVAR_ARCHIVE, NULL);
	m_interpolate = Cvar_Get("m_interpolate", "0", CVAR_ARCHIVE, NULL);
	m_invert = Cvar_Get("m_invert", "0", CVAR_ARCHIVE, "Invert the mouse");
	m_pitch = Cvar_Get("m_pitch", "0.022", 0, NULL);
	m_yaw = Cvar_Get("m_yaw", "0.022", 0, NULL);
}