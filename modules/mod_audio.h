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

#ifndef _TJOST_MOD_AUDIO_H_
#define _TJOST_MOD_AUDIO_H_

#include <tjost.h>

#define AUDIO_PATH "/audio"
#define AUDIO_FMT "iib"

typedef enum _Sample_Type Sample_Type;

enum _Sample_Type {
	SAMPLE_TYPE_UINT8		=   8,
	SAMPLE_TYPE_INT8		= - 8,
	SAMPLE_TYPE_UINT12	=  12,
	SAMPLE_TYPE_INT12		= -12,
	SAMPLE_TYPE_UINT16	=  16,
	SAMPLE_TYPE_INT16		= -16,
	SAMPLE_TYPE_UINT24	=  24,
	SAMPLE_TYPE_INT24		= -24,
	SAMPLE_TYPE_UINT32	=  32,
	SAMPLE_TYPE_INT32		= -32,
	SAMPLE_TYPE_FLOAT		=  INT32_MAX,
	SAMPLE_TYPE_DOUBLE	=  INT32_MIN
};

#endif
