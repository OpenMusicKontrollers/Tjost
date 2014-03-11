/*
 * Copyright (c) 2014 Hanspeter Portner (dev@open-music-kontrollers.ch)
 * 
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 
 *     1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 * 
 *     2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 * 
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 */

#include <math.h>

#include <tjost.h>

typedef struct _Midi_Client Midi_Client;
typedef struct _Midi_Blob Midi_Blob;

struct _Midi_Client {
	uint8_t effect;
	uint8_t double_precision;
	Eina_Mempool *pool;
	Eina_Inlist *blobs;

	float bot;
	float range;
};

struct _Midi_Blob {
	EINA_INLIST;
	uint32_t sid;
	uint8_t key;
};

//static uint8_t buffer [TJOST_BUF_SIZE] __attribute__((aligned (8)));
static Midi_Client midi;
static Midi_Client *midi_client = &midi;

static int
_on(jack_nframes_t time, const char *path, const char *fmt, lua_State *L)
{
	int i;
	uint32_t sid = luaL_checkint(L, 4);
	uint16_t uid = luaL_checkint(L, 5);
	uint16_t tid = luaL_checkint(L, 6);
	uint32_t gid = luaL_checkint(L, 7);
	float x = luaL_checknumber(L, 8);
	float y = luaL_checknumber(L, 9);
	float a = luaL_checknumber(L, 10);

	float key = midi_client->bot + x*midi_client->range;
	uint8_t base = floor(key);
	uint8_t vel = 0x7f;
	uint32_t bend = (key-base) / midi_client->range*0x2000 + 0x1fff;
	uint32_t eff = y*0x3fff;

	lua_pushvalue(L, lua_upvalueindex(1)); // push callback function
	{
		lua_pushnumber(L, time);
		lua_pushstring(L, "/midi");
		if(midi_client->double_precision)
			lua_pushstring(L, "mmmm");
		else
			lua_pushstring(L, "mmm");
		lua_createtable(L, 4, 0);
		{
			lua_pushnumber(L, gid);				
			lua_pushnumber(L, 0x90);		
			lua_pushnumber(L, base);			
			lua_pushnumber(L, vel);			
			for(i=0; i<4; i++)
				lua_rawseti(L, -5+i, 4-i);
		}
		lua_createtable(L, 4, 0);
		{
			lua_pushnumber(L, gid);			
			lua_pushnumber(L, 0xe0);		
			lua_pushnumber(L, bend & 0x7f);	
			lua_pushnumber(L, bend >> 7);	
			for(i=0; i<4; i++)
				lua_rawseti(L, -5+i, 4-i);
		}
		if(midi_client->double_precision)
		{
			lua_createtable(L, 4, 0);
			{
				lua_pushnumber(L, gid);
				lua_pushnumber(L, 0xb0);
				lua_pushnumber(L, midi_client->effect | 0x20);	
				lua_pushnumber(L, eff & 0x7f);	
				for(i=0; i<4; i++)
					lua_rawseti(L, -5+i, 4-i);
			}
		}
		lua_createtable(L, 4, 0);
		{
			lua_pushnumber(L, gid);	
			lua_pushnumber(L, 0xb0);
			lua_pushnumber(L, midi_client->effect);	
			lua_pushnumber(L, eff >> 7);	
			for(i=0; i<4; i++)
				lua_rawseti(L, -5+i, 4-i);
		}
		if(lua_pcall(L, midi_client->double_precision ? 7 : 6, 0, 0))
			fprintf(stderr, "Midi_Client 'on' error: %s\n", lua_tostring(L, -1));
	}

	Midi_Blob *b = eina_mempool_malloc(midi_client->pool, sizeof(Midi_Blob));
	b->sid = sid;
	b->key = key;
	midi_client->blobs = eina_inlist_append(midi_client->blobs, EINA_INLIST_GET(b));

	return 0;
}

static int
_off(jack_nframes_t time, const char *path, const char *fmt, lua_State *L)
{
	int i;
	uint32_t sid = luaL_checkint(L, 4);
	uint16_t uid = luaL_checkint(L, 5);
	uint16_t tid = luaL_checkint(L, 6);
	uint32_t gid = luaL_checkint(L, 7);

	Midi_Blob *b;
	EINA_INLIST_FOREACH(midi_client->blobs, b)
		if(b->sid == sid)
			break;
	
	uint8_t base = b->key;
	uint8_t vel = 0x7f;

	lua_pushvalue(L, lua_upvalueindex(1)); // push callback function
	{
		lua_pushnumber(L, time);
		lua_pushstring(L, "/midi");
		lua_pushstring(L, "m");
		lua_createtable(L, 4, 0);
		{
			lua_pushnumber(L, gid);				
			lua_pushnumber(L, 0x80);		
			lua_pushnumber(L, base);			
			lua_pushnumber(L, vel);			
			for(i=0; i<4; i++)
				lua_rawseti(L, -5+i, 4-i);
		}
		if(lua_pcall(L, 4, 0, 0))
			fprintf(stderr, "Midi_Client 'off' error: %s\n", lua_tostring(L, -1));
	}
	
	midi_client->blobs = eina_inlist_remove(midi_client->blobs, EINA_INLIST_GET(b));
	eina_mempool_free(midi_client->pool, b);

	return 0;
}

static int
_set(jack_nframes_t time, const char *path, const char *fmt, lua_State *L)
{
	int i;
	uint32_t sid = luaL_checkint(L, 4);
	uint16_t uid = luaL_checkint(L, 5);
	uint16_t tid = luaL_checkint(L, 6);
	uint32_t gid = luaL_checkint(L, 7);
	float x = luaL_checknumber(L, 8);
	float y = luaL_checknumber(L, 9);
	float a = luaL_checknumber(L, 10);

	Midi_Blob *b;
	EINA_INLIST_FOREACH(midi_client->blobs, b)
		if(b->sid == sid)
			break;

	float key = midi_client->bot + x*midi_client->range;
	uint8_t base = b->key;
	uint8_t vel = 0x7f;
	uint32_t bend = (key-base) / midi_client->range*0x2000 + 0x1fff;
	uint32_t eff = y*0x3fff;

	lua_pushvalue(L, lua_upvalueindex(1)); // push callback function
	{
		lua_pushnumber(L, time);
		lua_pushstring(L, "/midi");
		if(midi_client->double_precision)
			lua_pushstring(L, "mmm");
		else
			lua_pushstring(L, "mm");
		lua_createtable(L, 4, 0);
		{
			lua_pushnumber(L, gid);			
			lua_pushnumber(L, 0xe0);		
			lua_pushnumber(L, bend & 0x7f);	
			lua_pushnumber(L, bend >> 7);	
			for(i=0; i<4; i++)
				lua_rawseti(L, -5+i, 4-i);
		}
		if(midi_client->double_precision)
		{
			lua_createtable(L, 4, 0);
			{
				lua_pushnumber(L, gid);
				lua_pushnumber(L, 0xb0);
				lua_pushnumber(L, midi_client->effect | 0x20);	
				lua_pushnumber(L, eff & 0x7f);	
				for(i=0; i<4; i++)
					lua_rawseti(L, -5+i, 4-i);
			}
		}
		lua_createtable(L, 4, 0);
		{
			lua_pushnumber(L, gid);	
			lua_pushnumber(L, 0xb0);
			lua_pushnumber(L, midi_client->effect);	
			lua_pushnumber(L, eff >> 7);	
			for(i=0; i<4; i++)
				lua_rawseti(L, -5+i, 4-i);
		}
		if(lua_pcall(L, midi_client->double_precision ? 6 : 5, 0, 0))
			fprintf(stderr, "Midi_Client 'set' error: %s\n", lua_tostring(L, -1));
	}

	return 0;
}

static int
_idle(jack_nframes_t time, const char *path, const char *fmt, lua_State *L)
{
	//TODO

	return 0;
}

static int
_func(lua_State *L)
{	
	jack_nframes_t time = luaL_checkint(L, 1);
	const char *path = luaL_checkstring(L, 2);
	const char *fmt = luaL_checkstring(L, 3);

	if(!strcmp(path, "/on") && !strcmp(fmt, "iiiifff"))
		return _on(time, path, fmt, L);

	if(!strcmp(path, "/off") && !strcmp(fmt, "iiii"))
		return _off(time, path, fmt, L);

	if(!strcmp(path, "/set") && !strcmp(fmt, "iiiifff"))
		return _set(time, path, fmt, L);

	if(!strcmp(path, "/idle"))
		return _idle(time, path, fmt, L);

	return 0;
}

static int
_new(lua_State *L)
{
	uint8_t n = luaL_checkint(L, 1);
	//midi_client->effect = 0x07;
	//midi_client->double_precision = 1;
	midi_client->effect = 0x4a;
	midi_client->double_precision = 0;
	midi_client->bot = 2*12 - 0.5 - (fmod(n/3.f, 12.f) / 2.f);
	midi_client->range = n/3.f + 1.f;;

	if(lua_isfunction(L, 2) || lua_isuserdata(L, 2))
		lua_pushcclosure(L, _func, 1);
	else
		lua_pushnil(L);
	return 1;
}

int
luaopen_midi(lua_State *L)
{
	midi_client->pool = eina_mempool_add("chained_mempool", "blobs", NULL, sizeof(Midi_Blob), 32); //TODO free
	lua_pushcclosure(L, _new, 0);
	return 1;
}
