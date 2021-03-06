/*-
 * Copyright (c) 2011-2012 Juan Romero Pardines.
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/param.h>

#include <xbps_api.h>
#include "defs.h"

/*
 * Checks package integrity of an installed package.
 * The following task is accomplished in this file:
 *
 * 	o Check for missing run time dependencies.
 *
 * Returns 0 if test ran successfully, 1 otherwise and -1 on error.
 */

int
check_pkg_rundeps(struct xbps_handle *xhp,
		  const char *pkgname,
		  void *arg,
		  bool *pkgdb_update)
{
	prop_dictionary_t pkg_propsd = arg;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *reqpkg;
	bool test_broken = false;

	(void)pkgdb_update;

	if (!xbps_pkg_has_rundeps(pkg_propsd))
		return 0;

	iter = xbps_array_iter_from_dict(pkg_propsd, "run_depends");
	if (iter == NULL)
		return -1;

	while ((obj = prop_object_iterator_next(iter))) {
		reqpkg = prop_string_cstring_nocopy(obj);
		if (reqpkg == NULL) {
			prop_object_iterator_release(iter);
			return -1;
		}
		if (xbps_check_is_installed_pkg_by_pattern(xhp, reqpkg) <= 0) {
			xbps_error_printf("%s: dependency not satisfied: %s\n",
			    pkgname, reqpkg);
			test_broken = true;
		}
	}
	prop_object_iterator_release(iter);
	return test_broken;
}
