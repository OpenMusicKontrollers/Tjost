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

#define MAX_BLOB 16
typedef struct _Midi_Blob Midi_Blob;
typedef struct _Midi_Client Midi_Client;

struct _Midi_Blob {
	EINA_INLIST;
	uint32_t sid;
	uint8_t key;
};

struct _Midi_Client {
	float bot;
	float range;
	uint8_t effect;
	uint8_t double_precision;

	Eina_Inlist *blobs;

	int used [MAX_BLOB];
	Midi_Blob pool [MAX_BLOB];
};

static Midi_Blob *
blob_alloc(int *used, Midi_Blob *pool, size_t len)
{
	size_t i;
	for(i=0; i<len; i++)
		if(used[i] == 0)
		{
			used[i] = 1;
			return &pool[i];
		}
	return NULL;
}

static void
blob_free(int *used, Midi_Blob *pool, size_t len, Midi_Blob *blob)
{
	size_t i;
	for(i=0; i<len; i++)
		if(&pool[i] == blob)
		{
			used[i] = 0;
			break;
		}
}

static int
_on(jack_nframes_t time, const char *path, const char *fmt, lua_State *L)
{
	Midi_Client *midi_client = luaL_checkudata(L, lua_upvalueindex(2), "Fltr_Midi");

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

	Midi_Blob *b = blob_alloc(midi_client->used, midi_client->pool, MAX_BLOB);
	b->sid = sid;
	b->key = key;
	midi_client->blobs = eina_inlist_append(midi_client->blobs, EINA_INLIST_GET(b));

	return 0;
}

static int
_off(jack_nframes_t time, const char *path, const char *fmt, lua_State *L)
{
	Midi_Client *midi_client = luaL_checkudata(L, lua_upvalueindex(2), "Fltr_Midi");

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
	blob_free(midi_client->used, midi_client->pool, MAX_BLOB, b);

	return 0;
}

static int
_set(jack_nframes_t time, const char *path, const char *fmt, lua_State *L)
{
	Midi_Client *midi_client = luaL_checkudata(L, lua_upvalueindex(2), "Fltr_Midi");

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
	Midi_Client *midi_client = luaL_checkudata(L, lua_upvalueindex(2), "Fltr_Midi");
	//TODO

	return 0;
}

static int
_bottom(jack_nframes_t time, const char *path, const char *fmt, lua_State *L)
{
	Midi_Client *midi_client = luaL_checkudata(L, lua_upvalueindex(2), "Fltr_Midi");

	midi_client->bot = luaL_checknumber(L, 4);

	return 0;
}

static int
_range(jack_nframes_t time, const char *path, const char *fmt, lua_State *L)
{
	Midi_Client *midi_client = luaL_checkudata(L, lua_upvalueindex(2), "Fltr_Midi");

	midi_client->range = luaL_checknumber(L, 4);

	return 0;
}

static int
_effect(jack_nframes_t time, const char *path, const char *fmt, lua_State *L)
{
	Midi_Client *midi_client = luaL_checkudata(L, lua_upvalueindex(2), "Fltr_Midi");

	midi_client->effect = luaL_checkint(L, 4); // TODO check range

	return 0;
}

static int
_double_precision(jack_nframes_t time, const char *path, const char *fmt, lua_State *L)
{
	Midi_Client *midi_client = luaL_checkudata(L, lua_upvalueindex(2), "Fltr_Midi");

	midi_client->double_precision = lua_toboolean(L, 4);

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
		
	else if(!strcmp(path, "/off") && !strcmp(fmt, "iiii"))
		return _off(time, path, fmt, L);

	else if(!strcmp(path, "/set") && !strcmp(fmt, "iiiifff"))
		return _set(time, path, fmt, L);

	else if(!strcmp(path, "/idle"))
		return _idle(time, path, fmt, L);

	else if(!strcmp(path, "/bottom") && !strcmp(fmt, "f"))
		return _bottom(time, path, fmt, L);

	else if(!strcmp(path, "/range") && !strcmp(fmt, "f"))
		return _range(time, path, fmt, L);

	else if(!strcmp(path, "/effect") && !strcmp(fmt, "i"))
		return _effect(time, path, fmt, L);

	else if(!strcmp(path, "/double_precision") && !strcmp(fmt, "i"))
		return _double_precision(time, path, fmt, L);

	return 0;
}

static int
_new(lua_State *L)
{
	uint8_t n = 128;

	if(lua_isfunction(L, 1) || lua_isuserdata(L, 1))
	{
		Midi_Client *midi_client = lua_newuserdata(L, sizeof(Midi_Client));
		memset(midi_client, 0, sizeof(Midi_Client));
		luaL_getmetatable(L, "Fltr_Midi");
		lua_setmetatable(L, -2);

		midi_client->effect = 0x07;
		midi_client->double_precision = 1;
		midi_client->bot = 2*12 - 0.5 - ( (n % 18) / 6.f);
		midi_client->range = n/3.f;

		lua_pushcclosure(L, _func, 2);
	}
	else
		lua_pushnil(L);
	return 1;
}

static int
_gc(lua_State *L)
{
	printf("_gc_fltr_midi\n");
	Midi_Client *midi_client = luaL_checkudata(L, 1, "Fltr_Midi");

	return 0;
}

static const luaL_Reg fltr_midi_mt [] = {
	{"__gc", _gc},
	{NULL, NULL}
};

int
luaopen_midi(lua_State *L)
{
	luaL_newmetatable(L, "Fltr_Midi"); // mt
	luaL_register(L, NULL, fltr_midi_mt);
	lua_pop(L, 1); // mt

	lua_pushcclosure(L, _new, 0);
	return 1;
}
