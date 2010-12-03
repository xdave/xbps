/*-
 * Copyright (c) 2010 Juan Romero Pardines.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include <xbps_api.h>
#include "xbps_api_impl.h"

static bool with_debug;

void
xbps_init(bool debug)
{
	xbps_fetch_set_cache_connection(XBPS_FETCH_CACHECONN,
					XBPS_FETCH_CACHECONN_HOST);
	with_debug = debug;
}

void
xbps_end(void)
{
	xbps_regpkgdb_dictionary_release();
	xbps_repository_pool_release();
	xbps_fetch_unset_cache_connection();
}

void
xbps_dbg_printf(const char *fmt, ...)
{
	va_list ap;

	if (!with_debug)
		return;

	va_start(ap, fmt);
	fprintf(stderr, "[DEBUG] ");
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void
xbps_dbg_printf_append(const char *fmt, ...)
{
	va_list ap;

	if (!with_debug)
		return;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}