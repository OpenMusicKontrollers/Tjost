/*
 * Copyright (c) 2015 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#include <tjost.h>

#include <unistd.h> // gethostname

#define TJOST_BUNDLE_PUSH_PATH	"/bundle/push"
#define TJOST_BUNDLE_PUSH_FMT 	""
#define TJOST_BUNDLE_POP_PATH		"/bundle/pop"
#define TJOST_BUNDLE_POP_FMT 		""

static osc_data_t *
_push(Tjost_Host *host, osc_type_t type, osc_data_t *ptr)
{
	lua_State *L = host->L;

	switch(type)
	{
		case OSC_INT32:
		{
			int32_t i;
			ptr = osc_get_int32(ptr, &i);
			lua_pushnumber(L, i);
			return ptr;
		}
		case OSC_FLOAT:
		{
			float f;
			ptr = osc_get_float(ptr, &f);
			lua_pushnumber(L, f);
			return ptr;
		}
		case OSC_STRING:
		{
			const char *s;
			ptr = osc_get_string(ptr, &s);
			lua_pushstring(L, s);
			return ptr;
		}
		case OSC_BLOB:
		{
			osc_blob_t b;
			ptr = osc_get_blob(ptr, &b);

			Tjost_Blob *tb = lua_newuserdata(L, sizeof(Tjost_Blob) + b.size);
			luaL_getmetatable(L, "Tjost_Blob");
			lua_setmetatable(L, -2);

			tb->size = b.size;
			memcpy(tb->buf, b.payload, b.size);
			return ptr;
		}

		case OSC_INT64:
		{
			int64_t h;
			ptr = osc_get_int64(ptr, &h);
			lua_pushnumber(L, h);
			return ptr;
		}
		case OSC_DOUBLE:
		{
			double d;
			ptr = osc_get_double(ptr, &d);
			lua_pushnumber(L, d);
			return ptr;
		}
		case OSC_TIMETAG:
		{
			uint64_t t;
			ptr = osc_get_timetag(ptr, &t);
			lua_pushnumber(L, t);
			return ptr;
		}

		case OSC_TRUE:
			lua_pushboolean(L, 1);
			return ptr;
		case OSC_FALSE:
			lua_pushboolean(L, 0);
			return ptr;
		case OSC_BANG:
			lua_pushnumber(L, INT32_MAX);
			return ptr;
		case OSC_NIL:
			lua_pushnil(L);
			return ptr;

		case OSC_SYMBOL:
		{
			const char *S;
			ptr = osc_get_symbol(ptr, &S);
			lua_pushstring(L, S);
			return ptr;
		}
		case OSC_CHAR:
		{
			char c;
			ptr = osc_get_char(ptr, &c);
			lua_pushnumber(L, c);
			return ptr;
		}
		case OSC_MIDI:
		{
			uint8_t *m;
			ptr = osc_get_midi(ptr, &m);

			Tjost_Midi *tm = lua_newuserdata(L, sizeof(Tjost_Midi));
			luaL_getmetatable(L, "Tjost_Midi");
			lua_setmetatable(L, -2);

			tm->buf[0] = m[0];
			tm->buf[1] = m[1];
			tm->buf[2] = m[2];
			tm->buf[3] = m[3];
			return ptr;
		}

		default:
			tjost_host_message_push(host, "Lua: invalid argument type '%c'", type);
			return ptr;
	}
}

static int
_deserialize(osc_time_t time, const char *path, const char *fmt, osc_data_t *buf, size_t size, void *dat)
{
	Tjost_Module *module = dat;
	Tjost_Host *host = module->host;
	lua_State *L = host->L;

	osc_data_t *ptr = buf;
	int argc = 3 + strlen(fmt);
	
	if(!lua_checkstack(L, argc + 32)) // ensure at least that many free slots on stack
		tjost_host_message_push(host, "Lua: %s", "stack overflow");

	lua_pushlightuserdata(L, module);
	lua_rawget(L, LUA_REGISTRYINDEX); // responder function
	{
		lua_pushnumber(L, time);
		lua_pushstring(L, path);
		lua_pushstring(L, fmt);

		const char *type;
		for(type=fmt; *type!='\0'; type++)
			ptr = _push(host, *type, ptr);

		if(lua_pcall(L, argc, 0, 0))
			tjost_host_message_push(host, "Lua: callback error '%s'", lua_tostring(L, -1));
	}

	return 1;
}

static osc_method_t methods [] = {
	{NULL, NULL, _deserialize},
	{NULL, NULL, NULL}
};

static void
_bundle_in(osc_time_t time, void *dat)
{
	Tjost_Module *module = dat;
	Tjost_Host *host = module->host;
	lua_State *L = host->L;
	
	lua_pushlightuserdata(L, module);
	lua_rawget(L, LUA_REGISTRYINDEX); // responder function
	{
		lua_pushnumber(L, time);
		lua_pushstring(L, TJOST_BUNDLE_PUSH_PATH);
		lua_pushstring(L, TJOST_BUNDLE_PUSH_FMT);

		if(lua_pcall(L, 3, 0, 0))
			tjost_host_message_push(host, "Lua: callback error '%s'", lua_tostring(L, -1));
	}
}

static void
_bundle_out(osc_time_t time, void *dat)
{
	Tjost_Module *module = dat;
	Tjost_Host *host = module->host;
	lua_State *L = host->L;
	
	lua_pushlightuserdata(L, module);
	lua_rawget(L, LUA_REGISTRYINDEX); // responder function
	{
		lua_pushnumber(L, time);
		lua_pushstring(L, TJOST_BUNDLE_POP_PATH);
		lua_pushstring(L, TJOST_BUNDLE_POP_FMT);

		if(lua_pcall(L, 3, 0, 0))
			tjost_host_message_push(host, "Lua: callback error '%s'", lua_tostring(L, -1));
	}
}

void
tjost_lua_deserialize(Tjost_Event *tev)
{
	osc_dispatch_method(tev->time, tev->buf, tev->size, methods, _bundle_in, _bundle_out, tev->module);
}

static inline int
_serialize_packet(lua_State *L, Tjost_Module *module)
{
	Tjost_Host *host = module->host;
	osc_data_t *ptr = module->buf_ptr;
	osc_data_t *end = module->buffer + TJOST_BUF_SIZE;

	int has_timestamp = lua_isnumber(L, 2);
	int pos = 2 + has_timestamp;

	jack_nframes_t time = 0;
	if(has_timestamp)
		time = luaL_checkint(L, 2);
	const char *path = luaL_checkstring(L, pos);
	const char *fmt = luaL_checkstring(L, pos+1);

	if(!strcmp(path, TJOST_BUNDLE_PUSH_PATH) && !strcmp(fmt, TJOST_BUNDLE_PUSH_FMT))
	{
		int bundle_element = eina_inlist_count(module->bndls) > 0;
		if(!bundle_element)
			ptr = module->buffer;
		else // bundle_element
			ptr = osc_start_bundle_item(ptr, end, &module->itm);
		Tjost_Bundle *bndl = tjost_alloc(host, sizeof(Tjost_Bundle));
		module->bndls = eina_inlist_prepend(module->bndls, EINA_INLIST_GET(bndl));
		//bndl->time = time;
		ptr = osc_start_bundle(ptr, end, OSC_IMMEDIATE, &bndl->ptr); //FIXME how to handle timestamp?
	}
	else if(!strcmp(path, TJOST_BUNDLE_POP_PATH) && !strcmp(fmt, TJOST_BUNDLE_POP_FMT))
	{
		Tjost_Bundle *bndl = EINA_INLIST_CONTAINER_GET(module->bndls, Tjost_Bundle);
		module->bndls = eina_inlist_remove(module->bndls, EINA_INLIST_GET(bndl));
		ptr = osc_end_bundle(ptr, end, bndl->ptr);
		
		int bundle_element = eina_inlist_count(module->bndls) > 0;
		if(!bundle_element)
		{
			size_t size = ptr - module->buffer;
			if(size > 0)
				tjost_module_schedule(module, time, size, module->buffer);
		}
		else // bundle_element
			ptr = osc_end_bundle_item(ptr, end, module->itm);

		tjost_free(host, bndl);
	}
	else // normal message
	{
		osc_data_t *itm;

		int bundle_element = eina_inlist_count(module->bndls) > 0;
		if(!bundle_element)
			ptr = module->buffer;
		else // bundle_element
			ptr = osc_start_bundle_item(ptr, end, &itm);

		if(!osc_check_path(path))
		{
			tjost_host_message_push(host, "Lua: invalid OSC path %s", path);
			return 0;
		}
		ptr = osc_set_path(ptr, end, path);

		if(!osc_check_fmt(fmt, 0))
		{
			tjost_host_message_push(host, "Lua: invalid OSC format %s", fmt);
			return 0;
		}
		ptr = osc_set_fmt(ptr, end, fmt);

		int p = pos+2;
		const char *type;
		for(type=fmt; *type!='\0'; type++, p++)
			switch(*type)
			{
				case OSC_INT32:
					ptr = osc_set_int32(ptr, end, luaL_checkinteger(L, p));
					break;
				case OSC_FLOAT:
					ptr = osc_set_float(ptr, end, luaL_checknumber(L, p));
					break;
				case OSC_STRING:
					ptr = osc_set_string(ptr, end, luaL_checkstring(L, p));
					break;
				case OSC_BLOB:
					{
						Tjost_Blob *tb = luaL_checkudata(L, p, "Tjost_Blob");
						ptr = osc_set_blob(ptr, end, tb->size, tb->buf);
					}
					break;

				case OSC_INT64:
					ptr = osc_set_int64(ptr, end, luaL_checknumber(L, p));
					break;
				case OSC_DOUBLE:
					ptr = osc_set_double(ptr, end, luaL_checknumber(L, p));
					break;
				case OSC_TIMETAG:
					ptr = osc_set_timetag(ptr, end, luaL_checknumber(L, p));
					break;

				case OSC_TRUE:
				case OSC_FALSE:
				case OSC_NIL:
				case OSC_BANG:
					break;

				case OSC_SYMBOL:
					ptr = osc_set_symbol(ptr, end, luaL_checkstring(L, p));
					break;
				case OSC_MIDI:
					{
						Tjost_Midi *tm = luaL_checkudata(L, p, "Tjost_Midi");
						ptr = osc_set_midi(ptr, end, tm->buf);
					}
					break;
				case OSC_CHAR:
					ptr = osc_set_char(ptr, end, luaL_checknumber(L, p));
					break;

				default:
					tjost_host_message_push(host, "Lua: invalid argument type '%c'", *type);
					break;
			}

		if(!bundle_element)
		{
			size_t size = ptr - module->buffer;
			if(ptr && (size > 0))
				tjost_module_schedule(module, time, size, module->buffer);
		}
		else // bundle_element
			ptr = osc_end_bundle_item(ptr, end, itm);
	}
	
	module->buf_ptr = ptr;

	return 0;
}

static int
_call_output(lua_State *L)
{
	Tjost_Module *module = luaL_checkudata(L, 1, "Tjost_Output");
	return _serialize_packet(L, module);
}

static int
_call_in_out(lua_State *L)
{
	Tjost_Module *module = luaL_checkudata(L, 1, "Tjost_In_Out");
	return _serialize_packet(L, module);
}

static int
_call_uplink(lua_State *L)
{
	Tjost_Module *module = luaL_checkudata(L, 1, "Tjost_Uplink");
	return _serialize_packet(L, module);
}

static inline void
_clear(Tjost_Module *module)
{
	Tjost_Host *host = module->host;

	// drain event queue
	Eina_Inlist *l;
	Tjost_Event *tev;
	EINA_INLIST_FOREACH_SAFE(module->queue, l, tev)
	{
		module->queue = eina_inlist_remove(module->queue, EINA_INLIST_GET(tev));
		tjost_free(host, tev);
	}
}

static int
_gc_input(lua_State *L)
{
	//printf("_gc_input\n");
	Tjost_Module *module = luaL_checkudata(L, 1, "Tjost_Input");
	Tjost_Host *host = module->host;

	// clear responder function from registry
	lua_pushlightuserdata(L, module);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);
	lua_gc(host->L, LUA_GCSTEP, 0);

	module->del(module);
	host->modules = eina_inlist_remove(host->modules, EINA_INLIST_GET(module));

	Eina_Inlist *l;
	Tjost_Child *child;
	EINA_INLIST_FOREACH_SAFE(module->children, l, child)
	{
		module->children = eina_inlist_remove(module->children, EINA_INLIST_GET(child));
		tjost_free(host, child);
	}

	return 0;
}

static int
_gc_output(lua_State *L)
{
	//printf("_gc_output\n");
	Tjost_Module *module = luaL_checkudata(L, 1, "Tjost_Output");
	Tjost_Host *host = module->host;

	// clear responder function from registry
	lua_pushlightuserdata(L, module);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);
	lua_gc(host->L, LUA_GCSTEP, 0);

	module->del(module);
	_clear(module);
	host->modules = eina_inlist_remove(host->modules, EINA_INLIST_GET(module));

	return 0;
}

static int
_gc_in_out(lua_State *L)
{
	//printf("_gc_in_out\n");
	Tjost_Module *module = luaL_checkudata(L, 1, "Tjost_In_Out");
	Tjost_Host *host = module->host;

	// clear responder function from registry
	lua_pushlightuserdata(L, module);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);
	lua_gc(host->L, LUA_GCSTEP, 0);

	module->del(module);
	_clear(module);
	host->modules = eina_inlist_remove(host->modules, EINA_INLIST_GET(module));

	Eina_Inlist *l;
	Tjost_Child *child;
	EINA_INLIST_FOREACH_SAFE(module->children, l, child)
	{
		module->children = eina_inlist_remove(module->children, EINA_INLIST_GET(child));
		tjost_free(host, child);
	}

	return 0;
}

static int
_gc_uplink(lua_State *L)
{
	//printf("_gc_uplink\n");
	Tjost_Module *module = luaL_checkudata(L, 1, "Tjost_Uplink");
	Tjost_Host *host = module->host;

	// clear responder function from registry
	lua_pushlightuserdata(L, module);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);
	lua_gc(host->L, LUA_GCSTEP, 0);

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
_clear_in_out(lua_State *L)
{
	Tjost_Module *module = luaL_checkudata(L, 1, "Tjost_In_Out");
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

static int
_index_blob(lua_State *L)
{
	Tjost_Blob *tb = luaL_checkudata(L, 1, "Tjost_Blob");
	int typ = lua_type(L, 2);
	if(typ == LUA_TNUMBER)
	{
		int index = luaL_checkint(L, 2);
		if( (index >= 0) && (index < tb->size) )
			lua_pushnumber(L, tb->buf[index]);
		else
			lua_pushnil(L);
	}
	else if( (typ == LUA_TSTRING) && !strcmp(lua_tostring(L, 2), "raw") )
		lua_pushlightuserdata(L, tb->buf);
	else
		lua_pushnil(L);
	return 1;
}

static int
_newindex_blob(lua_State *L)
{
	Tjost_Blob *tb = luaL_checkudata(L, 1, "Tjost_Blob");
	int index = luaL_checkint(L, 2);
	if( (index >= 0) && (index < tb->size) )
		tb->buf[index] = luaL_checkint(L, 3);
	return 0;
}

static int
_len_blob(lua_State *L)
{
	Tjost_Blob *tb = luaL_checkudata(L, 1, "Tjost_Blob");
	lua_pushnumber(L, tb->size);
	return 1;
}

static int
_index_midi(lua_State *L)
{
	Tjost_Midi *tm = luaL_checkudata(L, 1, "Tjost_Midi");
	int typ = lua_type(L, 2);
	if(typ == LUA_TNUMBER)
	{
		int index = luaL_checkint(L, 2);
		if( (index >= 0) && (index < 4) )
			lua_pushnumber(L, tm->buf[index]);
		else
			lua_pushnil(L);
	}
	else if( (typ == LUA_TSTRING) && !strcmp(lua_tostring(L, 2), "raw") )
		lua_pushlightuserdata(L, tm->buf);
	else
		lua_pushnil(L);
	return 1;
}

static int
_newindex_midi(lua_State *L)
{
	Tjost_Midi *tm = luaL_checkudata(L, 1, "Tjost_Midi");
	int index = luaL_checkint(L, 2);
	if( (index >= 0) && (index < 4) )
		tm->buf[index] = luaL_checkint(L, 3);
	return 0;
}

static int
_len_midi(lua_State *L)
{
	Tjost_Midi *tm = luaL_checkudata(L, 1, "Tjost_Midi");
	lua_pushnumber(L, 4);
	return 1;
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

const luaL_Reg tjost_in_out_mt [] = {
	{"clear", _clear_in_out},
	{"__call", _call_in_out},
	{"__gc", _gc_in_out},
	{NULL, NULL}
};

const luaL_Reg tjost_uplink_mt [] = {
	{"clear", _clear_uplink},
	{"__call", _call_uplink},
	{"__gc", _gc_uplink},
	{NULL, NULL}
};

const luaL_Reg tjost_blob_mt [] = {
	{"__index", _index_blob},
	{"__newindex", _newindex_blob},
	{"__len", _len_blob},
	{NULL, NULL}
};

const luaL_Reg tjost_midi_mt [] = {
	{"__index", _index_midi},
	{"__newindex", _newindex_midi},
	{"__len", _len_midi},
	{NULL, NULL}
};

static int
_plugin(lua_State *L)
{
	Tjost_Host *host = lua_touserdata(L, lua_upvalueindex(1));

	Tjost_Module *module = lua_newuserdata(L, sizeof(Tjost_Module));
	memset(module, 0, sizeof(Tjost_Module));

	Eina_Module *mod;

	lua_getfield(L, 1, "name");
	const char *name = luaL_optstring(L, -1, NULL);
	lua_pop(L, 1);

	if(!(mod = eina_module_find(host->arr, name)))
		fprintf(stderr, "could not find module '%s'\n", name);
	if(!(module->add = eina_module_symbol_get(mod, "add")))
		fprintf(stderr, "could not get 'add' symbol\n");
	if(!(module->del = eina_module_symbol_get(mod, "del")))
		fprintf(stderr, "could not get 'del' symbol\n");
	module->process_in = NULL;
	module->process_out = NULL;

	module->host = host;

	// has a responder function ? TODO check Output of Uplink
	if(lua_gettop(L) > 2)
		switch(lua_type(L, 2))
		{
			case LUA_TTABLE: // TODO check for__call metamethod
			case LUA_TFUNCTION:
			{
				module->has_lua_callback = 1;

				lua_pushlightuserdata(L, module);
				lua_pushvalue(L, 2); // responder function
				lua_rawset(L, LUA_REGISTRYINDEX);
				break;
			}
			case LUA_TUSERDATA:
			{
				module->has_lua_callback = 0;

				Tjost_Module *mod_out = lua_touserdata(L, 2);
				if(mod_out->type & TJOST_MODULE_OUTPUT)
				{
					Tjost_Child *child = tjost_alloc(host, sizeof(Tjost_Child));
					child->module = mod_out;
					module->children = eina_inlist_append(module->children, EINA_INLIST_GET(child));
				}
				break;
			}
			default:
				break;
		}

	if(module->add(module))
	{
		lua_pop(L, 1);
		lua_pushnil(L);
	}

	switch(module->type)
	{
		case TJOST_MODULE_INPUT:
			if(!(module->process_in = eina_module_symbol_get(mod, "process_in")))
				fprintf(stderr, "could not get 'process_in' symbol\n");

			host->modules = eina_inlist_append(host->modules, EINA_INLIST_GET(module));

			luaL_getmetatable(L, "Tjost_Input");
			lua_setmetatable(L, -2);
			break;
		case TJOST_MODULE_OUTPUT:
			if(!(module->process_out = eina_module_symbol_get(mod, "process_out")))
				fprintf(stderr, "could not get 'process_out' symbol\n");

			host->modules = eina_inlist_append(host->modules, EINA_INLIST_GET(module));
			
			luaL_getmetatable(L, "Tjost_Output");
			lua_setmetatable(L, -2);
			break;
		case TJOST_MODULE_IN_OUT:
			if(!(module->process_in = eina_module_symbol_get(mod, "process_in")))
				fprintf(stderr, "could not get 'process_in' symbol\n");
			if(!(module->process_out = eina_module_symbol_get(mod, "process_out")))
				fprintf(stderr, "could not get 'process_out' symbol\n");

			host->modules = eina_inlist_append(host->modules, EINA_INLIST_GET(module));

			luaL_getmetatable(L, "Tjost_In_Out");
			lua_setmetatable(L, -2);
			break;
		case TJOST_MODULE_UPLINK:
			// uplinks get input from tjost main loop directly

			if(!(module->process_out = eina_module_symbol_get(mod, "process_out")))
				fprintf(stderr, "could not get 'process_out' symbol\n");

			host->uplinks = eina_inlist_append(host->uplinks, EINA_INLIST_GET(module));
			
			luaL_getmetatable(L, "Tjost_Uplink");
			lua_setmetatable(L, -2);
			break;
	}

	return 1;
}

static int
_chain(lua_State *L)
{
	Tjost_Module *mod_in = lua_touserdata(L, 1);
	Tjost_Module *mod_out = lua_touserdata(L, 2);
	Tjost_Host *host = mod_in->host;

	if( (mod_in->type & TJOST_MODULE_INPUT) && (mod_out->type & TJOST_MODULE_OUTPUT) )
	{
		Tjost_Child *child = tjost_alloc(host, sizeof(Tjost_Child));
		child->module = mod_out;
		mod_in->children = eina_inlist_append(mod_in->children, EINA_INLIST_GET(child));
	}
	else
		fprintf(stderr, "could not setup module chain\n");

	return 0;
}

static int
_blob(lua_State *L)
{
	int size = luaL_checkint(L, 1);

	Tjost_Blob *tb = lua_newuserdata(L, sizeof(Tjost_Blob) + size);
	memset(tb, 0, sizeof(Tjost_Blob) + size);
	tb->size = size;
	luaL_getmetatable(L, "Tjost_Blob");
	lua_setmetatable(L, -2);

	return 1;
}

static int
_midi(lua_State *L)
{
	Tjost_Midi *tm = lua_newuserdata(L, sizeof(Tjost_Midi));
	memset(tm, 0, 4);
	luaL_getmetatable(L, "Tjost_Midi");
	lua_setmetatable(L, -2);

	return 1;
}

static int
_hostname(lua_State *L)
{
	char hostname [256];
	gethostname(hostname, 256);
	if(hostname)
		lua_pushstring(L, hostname);
	else
		lua_pushnil(L);
	return 1;
}

const luaL_Reg tjost_globals [] = {
	{"plugin", _plugin},
	{"chain", _chain},
	{"blob", _blob},
	{"midi", _midi},
	{"hostname", _hostname},
	{NULL, NULL}
};

static int
_print(lua_State *L)
{
	Tjost_Host *host = lua_touserdata(L, lua_upvalueindex(1));

	if(lua_gettop(L) > 0)
	{
		while(lua_gettop(L) > 1)
		{
			lua_pushstring(L, "\t");
			lua_insert(L, lua_gettop(L) - 1);
			lua_concat(L, 3);
		}
		tjost_host_message_push(host, "%s", lua_tostring(L, 1));
	}

	return 0;
}

void
tjost_lua_init(Tjost_Host *host, int argc, const char **argv)
{
	lua_State *L = host->L;

	luaL_openlibs(L);

	// disable libs that are not rt safe.
	//lua_pushnil(L);
	//	lua_setglobal(L, "io"); //FIXME add function to get PID instead
	lua_pushnil(L);
		lua_setglobal(L, "os");
	lua_pushnil(L);
		lua_setglobal(L, "debug");

	// disable/overwrite funcs that are not rt safe.
	lua_pushnil(L);
		lua_setglobal(L, "loadfile");
	lua_pushnil(L);
		lua_setglobal(L, "dofile");
	lua_pushlightuserdata(L, host);
	lua_pushcclosure(L, _print, 1);
		lua_setglobal(L, "print");

	// create global reference for host
	lua_pushlightuserdata(L, host);
		lua_setglobal(L, "_H");

	// register Tjost methods
	lua_pushlightuserdata(L, host);
	luaL_openlib(L, "tjost", tjost_globals, 1);
	lua_pop(L, 1); // tjost 

	// register metatables
	luaL_newmetatable(L, "Tjost_Input"); // mt
	luaL_register(L, NULL, tjost_input_mt);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1); // mt

	luaL_newmetatable(L, "Tjost_Output"); // mt
	luaL_register(L, NULL, tjost_output_mt);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1); // mt

	luaL_newmetatable(L, "Tjost_In_Out"); // mt
	luaL_register(L, NULL, tjost_in_out_mt);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1); // mt

	luaL_newmetatable(L, "Tjost_Uplink"); // mt
	luaL_register(L, NULL, tjost_uplink_mt);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1); // mt

	luaL_newmetatable(L, "Tjost_Blob"); // mt
	luaL_register(L, NULL, tjost_blob_mt);
	lua_pop(L, 1); // mt

	luaL_newmetatable(L, "Tjost_Midi"); // mt
	luaL_register(L, NULL, tjost_midi_mt);
	lua_pop(L, 1); // mt

	lua_getglobal(L, "package");
	lua_getfield(L, -1, "cpath");
	lua_pushstring(L, ";/usr/local/lib/tjost/lua/?.so"); //FIXME
	lua_concat(L, 2);
	lua_setfield(L, -2, "cpath");
	lua_pop(L, 1); // package
	
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "path");
	lua_pushstring(L, ";/usr/local/bin/?.lua"); //FIXME
	lua_concat(L, 2);
	lua_setfield(L, -2, "path");
	lua_pop(L, 1); // package

	// push command line arguments
	lua_createtable(L, argc, 0);
	int i;
	for(i=2; i<argc; i++) {
		lua_pushstring(L, argv[i]);
		lua_rawseti(L, -2, i-1);
	}
	lua_setglobal(L, "argv");
}

void
tjost_lua_deinit(Tjost_Host *host)
{
	lua_State *L = host->L;

	if(L)
		lua_close(L);

	host-> L = NULL;
}

void
tjost_lua_deregister(Tjost_Host *host)
{
	lua_State *L = host->L;

	// deregister Tjost methods which are not rt safe
	lua_getglobal(L, "tjost");
	lua_pushnil(L);
		lua_setfield(L, -2, "plugin");
}
