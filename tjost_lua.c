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

static uint8_t buffer [TJOST_BUF_SIZE] __attribute__((aligned (8)));

static uint8_t *
_push(Tjost_Host *host, char type, uint8_t *ptr)
{
	lua_State *L = host->L;

	switch(type)
	{
		case 'i':
		{
			int32_t i;
			ptr = jack_osc_get_int32(ptr, &i);
			lua_pushnumber(L, i);
			return ptr;
		}
		case 'f':
		{
			float f;
			ptr = jack_osc_get_float(ptr, &f);
			lua_pushnumber(L, f);
			return ptr;
		}
		case 's':
		{
			const char *s;
			ptr = jack_osc_get_string(ptr, &s);
			lua_pushstring(L, s);
			return ptr;
		}
		case 'b':
		{
			Jack_OSC_Blob b;
			ptr = jack_osc_get_blob(ptr, &b);
			lua_createtable(L, b.size, 0);
			int i;
			for(i=0; i<b.size; i++)
			{
				lua_pushnumber(L, b.payload[i]);
				lua_rawseti(L, -2, i+1);
			}
			return ptr;
		}

		case 'h':
		{
			int64_t h;
			ptr = jack_osc_get_int64(ptr, &h);
			lua_pushnumber(L, h);
			return ptr;
		}
		case 'd':
		{
			double d;
			ptr = jack_osc_get_double(ptr, &d);
			lua_pushnumber(L, d);
			return ptr;
		}
		case 't':
		{
			uint64_t t;
			ptr = jack_osc_get_timetag(ptr, &t);
			lua_pushnumber(L, t);
			return ptr;
		}

		case 'T':
			lua_pushboolean(L, 1);
			return ptr;
		case 'F':
			lua_pushboolean(L, 0);
			return ptr;
		case 'I':
			lua_pushnumber(L, INT32_MAX);
			return ptr;
		case 'N':
			lua_pushnil(L);
			return ptr;

		case 'S':
		{
			const char *S;
			ptr = jack_osc_get_symbol(ptr, &S);
			lua_pushstring(L, S);
			return ptr;
		}
		case 'c':
		{
			char c;
			ptr = jack_osc_get_char(ptr, &c);
			lua_pushnumber(L, c);
			return ptr;
		}
		case 'm':
		{
			uint8_t *m;
			ptr = jack_osc_get_midi(ptr, &m);
			lua_createtable(L, 4, 0);
			lua_pushnumber(L, m[0]);	lua_rawseti(L, -2, 1);
			lua_pushnumber(L, m[1]);	lua_rawseti(L, -2, 2);
			lua_pushnumber(L, m[2]);	lua_rawseti(L, -2, 3);
			lua_pushnumber(L, m[3]);	lua_rawseti(L, -2, 4);
			return ptr;
		}

		default:
			tjost_host_message_push(host, "Lua: invalid argument type '%c'", type);
			return ptr;
	}
}

void
tjost_lua_deserialize_unicast(Tjost_Event *tev)
{
	//printf("deserialize_unicast\n");
	Tjost_Module *module = tev->module;
	Tjost_Host *host = module->host;
	lua_State *L = host->L;

	uint8_t *ptr = tev->buf;

	const char *path;
	const char *fmt;

	ptr = jack_osc_get_path(ptr, &path);
	ptr = jack_osc_get_fmt(ptr, &fmt);

	int argc = 3 + strlen(fmt);
	
	if(!lua_checkstack(L, argc + 32)) // ensure at least that many free slots on stack
		tjost_host_message_push(host, "Lua: %s", "stack overflow");

	lua_pushlightuserdata(L, module);
	lua_rawget(L, LUA_REGISTRYINDEX); // responder function
	{
		lua_pushnumber(L, tev->time);
		lua_pushstring(L, path);
		lua_pushstring(L, fmt);

		const char *type;
		for(type=fmt; *type!='\0'; type++)
			ptr = _push(host, *type, ptr);

		if(lua_pcall(L, argc, 0, 0))
			tjost_host_message_push(host, "Lua: callback error '%s'", lua_tostring(L, -1));
	}
}

void
tjost_lua_deserialize_broadcast(Tjost_Event *tev, Eina_Inlist *modules)
{
	//printf("deserialize_broadcast\n");
	uint8_t *ptr = tev->buf;

	const char *path;
	const char *fmt;

	ptr = jack_osc_get_path(ptr, &path);
	ptr = jack_osc_get_fmt(ptr, &fmt);

	Tjost_Module *module;
	EINA_INLIST_FOREACH(modules, module)
	{
		Tjost_Host *host = module->host;
		lua_State *L = host->L;

		lua_pushlightuserdata(L, module);
		lua_rawget(L, LUA_REGISTRYINDEX); // responder function
		{
			lua_pushnumber(L, tev->time);
			lua_pushstring(L, path);
			lua_pushstring(L, fmt);

			const char *type;
			for(type=fmt; *type!='\0'; type++)
				ptr = _push(host, *type, ptr);

			if(lua_pcall(L, 3 + strlen(fmt), 0, 0))
				tjost_host_message_push(host, "Lua: callback error '%s'", lua_tostring(L, -1));
		}
	}
}

static inline int
_serialize(lua_State *L, Tjost_Module *module)
{
	Tjost_Host *host = module->host;

	jack_nframes_t time = luaL_checkinteger(L, 2);
	const char *path = luaL_checkstring(L, 3);
	const char *fmt = luaL_checkstring(L, 4);

	size_t len;

	uint8_t *ptr = buffer;

	ptr = jack_osc_set_path(ptr, path);
	ptr = jack_osc_set_fmt(ptr, fmt);

	int p = 5;
	const char *type;
	for(type=fmt; *type!='\0'; type++, p++)
		switch(*type)
		{
			case 'i':
				ptr = jack_osc_set_int32(ptr, luaL_checkinteger(L, p));
				break;
			case 'f':
				ptr = jack_osc_set_float(ptr, luaL_checknumber(L, p));
				break;
			case 's':
				ptr = jack_osc_set_string(ptr, luaL_checkstring(L, p));
				break;
			case 'b':
				if(lua_istable(L, p))
				{
					// is more efficient than jack_osc_set_blob...
					size_t size = lua_objlen(L, p);
					size_t len = round_to_four_bytes(size);
					*(int32_t *)ptr = htonl(size);
					ptr += 4;

					int i;
					for(i=0; i<size; i++)
					{
						lua_rawgeti(L, p, i+1);
						ptr[i] = luaL_checkinteger(L, -1);
						lua_pop(L, 1);
					}
					memset(ptr+size, '\0', len-size); // zero padding
					ptr += len;
				}
				else
					tjost_host_message_push(host, "Lua: table expected at argument position %i", p);
				break;

			case 'h':
				ptr = jack_osc_set_int64(ptr, luaL_checknumber(L, p));
				break;
			case 'd':
				ptr = jack_osc_set_double(ptr, luaL_checknumber(L, p));
				break;
			case 't':
				ptr = jack_osc_set_timetag(ptr, luaL_checknumber(L, p));
				break;

			case 'T':
			case 'F':
			case 'N':
			case 'I':
				break;

			case 'S':
				ptr = jack_osc_set_symbol(ptr, luaL_checkstring(L, p));
				break;
			case 'm':
				if(lua_istable(L, p))
				{
					// is more efficient than jack_osc_set_midi...
					lua_rawgeti(L, p, 1);
					lua_rawgeti(L, p, 2);
					lua_rawgeti(L, p, 3);
					lua_rawgeti(L, p, 4);
					ptr[0] = luaL_checkinteger(L, -4);
					ptr[1] = luaL_checkinteger(L, -3);
					ptr[2] = luaL_checkinteger(L, -2);
					ptr[3] = luaL_checkinteger(L, -1);
					lua_pop(L, 4);
					ptr += 4;
				}
				else
					tjost_host_message_push(host, "Lua: table expected at argument position %i", p);
				break;
			case 'c':
				ptr = jack_osc_set_char(ptr, luaL_checknumber(L, p));
				break;

			default:
				tjost_host_message_push(host, "Lua: invalid argument type '%c'", *type);
				break;
		}

	size_t size = ptr - buffer;

	tjost_module_schedule(module, time, size, buffer);

	return 0;
}

static int
_call_output(lua_State *L)
{
	Tjost_Module *module = luaL_checkudata(L, 1, "Tjost_Output");
	return _serialize(L, module);
}

static int
_call_uplink(lua_State *L)
{
	Tjost_Module *module = luaL_checkudata(L, 1, "Tjost_Uplink");
	return _serialize(L, module);
}

static inline void
_clear(Tjost_Module *module)
{
	Tjost_Host *host = module->host;

	// drain event queue
	Eina_Inlist *itm;
	EINA_INLIST_FREE(module->queue, itm)
	{
		Tjost_Event *tev = EINA_INLIST_CONTAINER_GET(itm, Tjost_Event);

		module->queue = eina_inlist_remove(module->queue, itm);
		tjost_free(host, tev);
	}
}

static int
_gc_input(lua_State *L)
{
	printf("_gc_input\n");
	Tjost_Module *module = luaL_checkudata(L, 1, "Tjost_Input");
	Tjost_Host *host = module->host;

	// clear responder function from registry
	lua_pushlightuserdata(L, module);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);

	module->del(module);
	host->inputs = eina_inlist_remove(host->inputs, EINA_INLIST_GET(module));

	return 0;
}

static int
_gc_output(lua_State *L)
{
	printf("_gc_output\n");
	Tjost_Module *module = luaL_checkudata(L, 1, "Tjost_Output");
	Tjost_Host *host = module->host;

	// clear responder function from registry
	lua_pushlightuserdata(L, module);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);

	module->del(module);
	_clear(module);
	host->outputs = eina_inlist_remove(host->outputs, EINA_INLIST_GET(module));

	return 0;
}

static int
_gc_uplink(lua_State *L)
{
	printf("_gc_uplink\n");
	Tjost_Module *module = luaL_checkudata(L, 1, "Tjost_Uplink");
	Tjost_Host *host = module->host;

	// clear responder function from registry
	lua_pushlightuserdata(L, module);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);

	module->del(module);
	_clear(module);
	host->uplinks = eina_inlist_remove(host->uplinks, EINA_INLIST_GET(module));

	return 0;
}

static int
_clear_output(lua_State *L)
{
	Tjost_Module *module = luaL_checkudata(L, 1, "Tjost_Output");
	_clear(module);
	return 0;
}

static int
_clear_uplink(lua_State *L)
{
	Tjost_Module *module = luaL_checkudata(L, 1, "Tjost_Uplink");
	_clear(module);
	return 0;
}

const luaL_Reg tjost_input_mt [] = {
	{"__gc", _gc_input},
	{NULL, NULL}
};

const luaL_Reg tjost_output_mt [] = {
	{"clear", _clear_output},
	{"__call", _call_output},
	{"__gc", _gc_output},
	{NULL, NULL}
};

const luaL_Reg tjost_uplink_mt [] = {
	{"clear", _clear_uplink},
	{"__call", _call_uplink},
	{"__gc", _gc_uplink},
	{NULL, NULL}
};

//TODO deactivate while running
static int
_plugin(lua_State *L)
{
	int argc = lua_gettop(L);
	const char **argv = calloc(argc, sizeof(const char *));

	int i;
	for(i=0; i<argc; i++)
		if(lua_isstring(L, i+1))
			argv[i] = lua_tostring(L, i+1);
		else
			break;

	Tjost_Host *host = lua_touserdata(L, lua_upvalueindex(1));

	Tjost_Module *module = lua_newuserdata(L, sizeof(Tjost_Module));
	bzero(module, sizeof(Tjost_Module));

	Eina_Module *mod;

	if(!(mod = eina_module_find(host->arr, argv[0])))
		fprintf(stderr, "could not find module '%s'\n", argv[0]);
	if(!(module->add = eina_module_symbol_get(mod, "add")))
		fprintf(stderr, "could not get 'add' symbol\n");
	if(!(module->del = eina_module_symbol_get(mod, "del")))
		fprintf(stderr, "could not get 'del' symbol\n");
	if(!(module->process = eina_module_symbol_get(mod, "process")))
		fprintf(stderr, "could not get 'process' symbol\n");

	module->host = host;

	module->add(module, argc-1, argv+1); //FIXME wrong argc when callback function present

	free(argv);

	if(lua_isfunction(L, -2) || lua_isuserdata(L, -2)) // has a responder function ? TODO check Output of Uplink
	{
		// put responder function | user data into registry
		lua_pushlightuserdata(L, module);
		lua_pushvalue(L, -3); // responder function
		lua_rawset(L, LUA_REGISTRYINDEX);
	}

	switch(module->type)
	{
		case TJOST_MODULE_INPUT:
			host->inputs = eina_inlist_append(host->inputs, EINA_INLIST_GET(module));

			luaL_getmetatable(L, "Tjost_Input");
			lua_setmetatable(L, -2);
			break;
		case TJOST_MODULE_OUTPUT:
			host->outputs = eina_inlist_append(host->outputs, EINA_INLIST_GET(module));
			
			luaL_getmetatable(L, "Tjost_Output");
			lua_setmetatable(L, -2);
			break;
		case TJOST_MODULE_UPLINK:
			host->uplinks = eina_inlist_append(host->uplinks, EINA_INLIST_GET(module));
			
			luaL_getmetatable(L, "Tjost_Uplink");
			lua_setmetatable(L, -2);
			break;
	}

	return 1;
}

const luaL_Reg tjost_globals [] = {
	{"plugin", _plugin},
	{NULL, NULL}
};
