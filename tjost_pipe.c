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

static void
_asio(uv_async_t *handle)
{
	Tjost_Pipe *pipe = (Tjost_Pipe *)handle->data;

	tjost_pipe_consume(pipe, pipe->alloc_cb, pipe->sched_cb, pipe->arg);
}

int
tjost_pipe_init(Tjost_Pipe *pipe)
{
	// init jack ringbuffer
	if(!(pipe->rb = jack_ringbuffer_create(TJOST_RINGBUF_SIZE)))
		return -1;

	return 0;
}

int 
tjost_pipe_deinit(Tjost_Pipe *pipe)
{
	// deinit jack ringbuffer
	if(pipe->rb)
		jack_ringbuffer_free(pipe->rb);

	return 0;
}

size_t
tjost_pipe_space(Tjost_Pipe *pipe)
{
	return jack_ringbuffer_write_space(pipe->rb);
}

int
tjost_pipe_produce(Tjost_Pipe *pipe, jack_nframes_t timestamp, size_t len, osc_data_t *buf)
{
	Tjost_Event tev;
	tev.time = timestamp;
	tev.size = len;

	if(jack_ringbuffer_write_space(pipe->rb) < sizeof(Tjost_Event) + tev.size)
		return -1;
	else
	{
		if(jack_ringbuffer_write(pipe->rb, (const char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
			return -1;
		if(jack_ringbuffer_write(pipe->rb, (const char *)buf, tev.size) != tev.size)
			return -1;
	}

	return 0;
}

int
tjost_pipe_flush(Tjost_Pipe *pipe)
{
	if(uv_async_send(&pipe->asio))
		return -1;

	return 0;
}

int
tjost_pipe_consume(Tjost_Pipe *pipe, Tjost_Pipe_Alloc_Cb alloc_cb, Tjost_Pipe_Sched_Cb sched_cb, void *arg)
{
	Tjost_Event tev;

	while(jack_ringbuffer_read_space(pipe->rb) >= sizeof(Tjost_Event))
	{
		if(jack_ringbuffer_peek(pipe->rb, (char *)&tev, sizeof(Tjost_Event)) != sizeof(Tjost_Event))
			return -1;

		if(jack_ringbuffer_read_space(pipe->rb) >= sizeof(Tjost_Event) + tev.size)
		{
			jack_ringbuffer_read_advance(pipe->rb, sizeof(Tjost_Event));

			osc_data_t *buffer = alloc_cb(tev.time, tev.size, arg);
			//FIXME check return

			if(jack_ringbuffer_read(pipe->rb, (char *)buffer, tev.size) != tev.size)
				return -1;

			if(sched_cb(tev.time, buffer, tev.size, arg))
				break;
		}
	}

	return 0;
}

int
tjost_pipe_listen_start(Tjost_Pipe *pipe, uv_loop_t *loop, Tjost_Pipe_Alloc_Cb alloc_cb, Tjost_Pipe_Sched_Cb sched_cb, void *arg)
{
	pipe->alloc_cb = alloc_cb;
	pipe->sched_cb = sched_cb;
	pipe->arg = arg;

	// init uv asynchronous signal
	pipe->asio.data = pipe;
	int err;
	if((err = uv_async_init(loop, &pipe->asio, _asio)))
		fprintf(stderr, "tjost_pipe_init: %s\n", uv_err_name(err));

	return 0;
}

int
tjost_pipe_listen_stop(Tjost_Pipe *pipe)
{
	// deinit uv asynchronous signal
	uv_close((uv_handle_t *)&pipe->asio, NULL);

	return 0;
}
