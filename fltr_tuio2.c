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

#include <tjost.h>

#define FIRST_FRAME 1U

#define MAX_BLOB 32
typedef struct _Tuio2_Blob Tuio2_Blob;
typedef struct _Tuio2_Client Tuio2_Client;

struct _Tuio2_Blob {
	EINA_INLIST;
	uint32_t sid;
	uint32_t gid;
	uint16_t tid;
	uint16_t uid;
	float x, z, a;
	int is_new;
};

struct _Tuio2_Client {
	uint32_t fid;
	uint64_t timestamp;
	/*
	const char *app;
	uint8_t addr;
	uint32_t inst;
	uint16_t w;
	uint16_t h;
	*/
	uint32_t missing;
	uint8_t ignore;
	
	Eina_Inlist *blobs;

	int used [MAX_BLOB];
	Tuio2_Blob pool [MAX_BLOB];
};

static Tuio2_Blob *
blob_alloc(int *used, Tuio2_Blob *pool, size_t len)
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
blob_free(int *used, Tuio2_Blob *pool, size_t len, Tuio2_Blob *blob)
{
	size_t i;
	for(i=0; i<len; i++)
		if(&pool[i] == blob)
			used[i] = 0;
}

static int
_frm(jack_nframes_t time, const char *path, const char *fmt, lua_State *L)
{
	Tuio2_Client *tuio2_client = luaL_checkudata(L, lua_upvalueindex(2), "Fltr_Tuio2");

	uint32_t fid = luaL_checkint(L, 4);
	uint64_t timestamp = luaL_checknumber(L, 5);

	tuio2_client->ignore = 0; // this is a new bundle, so reset ignore state

	if(fid == FIRST_FRAME) // the tracker has been reset, we should reset our state, too
	{
		Tuio2_Blob *b;
		EINA_INLIST_FOREACH(tuio2_client->blobs, b)
			blob_free(tuio2_client->used, tuio2_client->pool, MAX_BLOB, b);
		tuio2_client->blobs = NULL;
		tuio2_client->missing = 0;
		tuio2_client->fid = FIRST_FRAME;
		//fprintf(stderr, "chimaera has been reset, memux Tuio2_Client is being reset, too\n");
	}
	else if( (fid < tuio2_client->fid) && (tuio2_client->missing > 0))
	{
		tuio2_client->ignore = 1; // ignore this bundle
		tuio2_client->missing -= 1;
		//fprintf(stderr, "found missing bundle #%u, total missing bundles: %u\n", fid, tuio2_client->missing);

		return 0;
	}
	else if( (tuio2_client->fid != FIRST_FRAME) && (fid > tuio2_client->fid + 1))
	{
		tuio2_client->missing += fid - tuio2_client->fid - 1;
		/*
		fprintf(stderr, "%u bundles (#%u-#%u) were just found to be missing, total missing bundles: %u\n",
			fid - tuio2_client->fid - 1,
			tuio2_client->fid,
			fid - 1,
			tuio2_client->missing);
		*/
	}

	tuio2_client->fid = fid;

	return 0;
}

static int
_tok(jack_nframes_t time, const char *path, const char *fmt, lua_State *L)
{
	Tuio2_Client *tuio2_client = luaL_checkudata(L, lua_upvalueindex(2), "Fltr_Tuio2");

	if(tuio2_client->ignore)
		return 0;

	uint32_t sid = luaL_checkint(L, 4);

	Tuio2_Blob *ptr;
	int exists = 0;

	// does blob with this sid already exist?
	EINA_INLIST_FOREACH(tuio2_client->blobs, ptr)
		if(ptr->sid == sid)
		{
			exists = 1;
			break;
		}

	// if not existing create it in the first place
	if(!exists)
	{
		ptr = blob_alloc(tuio2_client->used, tuio2_client->pool, MAX_BLOB);
		ptr->is_new = 1;
		tuio2_client->blobs = eina_inlist_append(tuio2_client->blobs, EINA_INLIST_GET(ptr));
	}

	uint32_t tuid = luaL_checkint(L, 5);
	uint32_t gid = luaL_checkint(L, 6);
	float x = luaL_checknumber(L, 7);
	float y = luaL_checknumber(L, 8);
	float a = luaL_checknumber(L, 9);

	ptr->sid = sid;
	ptr->uid = tuid >> 16;
	ptr->tid = tuid & 0xffff;
	ptr->gid = gid;
	ptr->x = x;
	ptr->z = y;
	ptr->a = a;

	return 0;
}

static int
_alv(jack_nframes_t time, const char *path, const char *fmt, lua_State *L)
{
	Tuio2_Client *tuio2_client = luaL_checkudata(L, lua_upvalueindex(2), "Fltr_Tuio2");

	if(tuio2_client->ignore)
		return 0;

	int argc = strlen(fmt);

	if( (argc == 0) && (eina_inlist_count(tuio2_client->blobs) == 0) )
	{
		lua_pushvalue(L, lua_upvalueindex(1)); // push callback function
		{
			lua_pushnumber(L, time);
			lua_pushstring(L, "/idle");
			lua_pushstring(L, "");
			if(lua_pcall(L, 3, 0, 0))
				fprintf(stderr, "Tuio2_Client 'idle' error: %s\n", lua_tostring(L, -1)); //TODO tjost_message
		}
		return 0;
	}

	Eina_Inlist *l;
	Tuio2_Blob *ptr;

	// search for disappeared blobs
	EINA_INLIST_FOREACH_SAFE(tuio2_client->blobs, l, ptr)
	{
		int i;
		int active = 0;
		for(i=0; i<argc; i++)
		{
			uint32_t sid = luaL_checkint(L, 4+i);
			if(sid == ptr->sid)
			{
				active = 1;
				break;
			}
		}
		if(!active) // blob has disappeared
		{
			lua_pushvalue(L, lua_upvalueindex(1)); // push callback function
			{
				lua_pushnumber(L, time);
				lua_pushstring(L, "/off");
				lua_pushstring(L, "iiii");
				lua_pushnumber(L, ptr->sid);
				lua_pushnumber(L, ptr->uid);
				lua_pushnumber(L, ptr->tid);
				lua_pushnumber(L, ptr->gid);
				if(lua_pcall(L, 7, 0, 0))
					fprintf(stderr, "Tuio2_Client 'off' error: %s\n", lua_tostring(L, -1)); //TODO tjost_message
			}

			tuio2_client->blobs = eina_inlist_remove(tuio2_client->blobs, EINA_INLIST_GET(ptr));
			blob_free(tuio2_client->used, tuio2_client->pool, MAX_BLOB, ptr);
		}
	}

	// search for newly occured blobs
	EINA_INLIST_FOREACH(tuio2_client->blobs, ptr)
	{
		if(ptr->is_new)
		{
			ptr->is_new = 0;

			lua_pushvalue(L, lua_upvalueindex(1)); // push callback function
			{
				lua_pushnumber(L, time);
				lua_pushstring(L, "/on");
				lua_pushstring(L, "iiiifff");
				lua_pushnumber(L, ptr->sid);
				lua_pushnumber(L, ptr->uid);
				lua_pushnumber(L, ptr->tid);
				lua_pushnumber(L, ptr->gid);
				lua_pushnumber(L, ptr->x);
				lua_pushnumber(L, ptr->z);
				lua_pushnumber(L, ptr->a);
				if(lua_pcall(L, 10, 0, 0))
					fprintf(stderr, "Tuio2_Client 'on' error: %s\n", lua_tostring(L, -1)); //TODO tjost_message
			}
		}
		else // update the state
		{
			lua_pushvalue(L, lua_upvalueindex(1)); // push callback function
			{
				lua_pushnumber(L, time);
				lua_pushstring(L, "/set");
				lua_pushstring(L, "iiiifff");
				lua_pushnumber(L, ptr->sid);
				lua_pushnumber(L, ptr->uid);
				lua_pushnumber(L, ptr->tid);
				lua_pushnumber(L, ptr->gid);
				lua_pushnumber(L, ptr->x);
				lua_pushnumber(L, ptr->z);
				lua_pushnumber(L, ptr->a);
				if(lua_pcall(L, 10, 0, 0))
					fprintf(stderr, "Tuio2_Client 'set' error: %s\n", lua_tostring(L, -1)); //TODO tjost_message
			}
		}
	}

	return 0;
}

static int
_func(lua_State *L)
{	
	jack_nframes_t time = luaL_checkint(L, 1);
	const char *path = luaL_checkstring(L, 2);
	const char *fmt = luaL_checkstring(L, 3);

	if(!strcmp(path, "/tuio2/frm") && !strcmp(fmt, "it"))
		return _frm(time, path, fmt, L);

	if(!strcmp(path, "/tuio2/tok") && !strcmp(fmt, "iiifff"))
		return _tok(time, path, fmt, L);

	if(!strcmp(path, "/tuio2/alv"))
		return _alv(time, path, fmt, L);

	return 0;
}

static int
_new(lua_State *L)
{
	if(lua_isfunction(L, 1) || lua_isuserdata(L, 1))
	{
		Tuio2_Client *tuio2_client = lua_newuserdata(L, sizeof(Tuio2_Client));
		memset(tuio2_client, 0, sizeof(Tuio2_Client));
		luaL_getmetatable(L, "Fltr_Tuio2");
		lua_setmetatable(L, -2);

		lua_pushcclosure(L, _func, 2);
	}
	else
		lua_pushnil(L);
	return 1;
}

static int
_gc(lua_State *L)
{
	printf("_gc_fltr_tuio2\n");
	Tuio2_Client *tuio2_client = luaL_checkudata(L, 1, "Fltr_Tuio2");

	return 0;
}

static const luaL_Reg fltr_tuio2_mt [] = {
	{"__gc", _gc},
	{NULL, NULL}
};

int
luaopen_tuio2(lua_State *L)
{
	luaL_newmetatable(L, "Fltr_Tuio2"); // mt
	luaL_register(L, NULL, fltr_tuio2_mt);
	lua_pop(L, 1); // mt

	lua_pushcclosure(L, _new, 0);
	return 1;
}
