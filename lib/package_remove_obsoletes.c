/*-
 * Copyright (c) 2009-2012 Juan Romero Pardines.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>

#include "xbps_api_impl.h"

int HIDDEN
xbps_remove_obsoletes(struct xbps_handle *xhp,
		      const char *pkgname,
		      const char *version,
		      const char *pkgver,
		      prop_dictionary_t oldd,
		      prop_dictionary_t newd)
{
	prop_object_iterator_t iter, iter2;
	prop_object_t obj, obj2;
	prop_string_t oldstr, newstr;
	struct stat st;
	const char *array_str = "files";
	const char *oldhash;
	char *file;
	int rv = 0;
	bool found, dodirs = false, dolinks = false;

	assert(prop_object_type(oldd) == PROP_TYPE_DICTIONARY);
	assert(prop_object_type(newd) == PROP_TYPE_DICTIONARY);

again:
	iter = xbps_array_iter_from_dict(oldd, array_str);
	if (iter == NULL)
		goto out1;
	iter2 = xbps_array_iter_from_dict(newd, array_str);
	if (iter2 == NULL)
		goto out1;

	/*
	 * Check for obsolete files, i.e files/links/dirs available in
	 * the old package list not found in new package list.
	 */
	while ((obj = prop_object_iterator_next(iter))) {
		rv = 0;
		found = false;
		oldstr = prop_dictionary_get(obj, "file");
		if (oldstr == NULL) {
			rv = errno;
			goto out;
		}
		file = xbps_xasprintf(".%s",
		    prop_string_cstring_nocopy(oldstr));
		if (file == NULL) {
			rv = errno;
			goto out;
		}
		if (strcmp(array_str, "files") == 0) {
			prop_dictionary_get_cstring_nocopy(obj,
			    "sha256", &oldhash);
			rv = xbps_file_hash_check(file, oldhash);
			if (rv == ENOENT || rv == ERANGE) {
				/*
				 * Skip unexistent and files that do not
				 * match the hash.
				 */
				free(file);
				rv = 0;
				continue;
			}
		} else if (strcmp(array_str, "links") == 0) {
			/*
			 * Only remove dangling symlinks.
			 */
			if (stat(file, &st) == -1) {
				if (errno != ENOENT) {
					free(file);
					rv = errno;
					goto out;
				}
			} else {
				free(file);
				continue;
			}
		}

		while ((obj2 = prop_object_iterator_next(iter2))) {
			newstr = prop_dictionary_get(obj2, "file");
			if (newstr == NULL) {
				rv = errno;
				goto out;
			}
			/*
			 * Skip files with same path.
			 */
			if (prop_string_equals(oldstr, newstr)) {
				found = true;
				break;
			}
		}
		prop_object_iterator_reset(iter2);
		if (found) {
			free(file);
			continue;
		}
		/*
		 * Do not remove required symlinks for the
		 * system transition to /usr.
		 */
		if ((strcmp(file, "./bin") == 0) ||
		    (strcmp(file, "./bin/") == 0) ||
		    (strcmp(file, "./sbin") == 0) ||
		    (strcmp(file, "./sbin/") == 0) ||
		    (strcmp(file, "./lib") == 0) ||
		    (strcmp(file, "./lib/") == 0) ||
		    (strcmp(file, "./lib64/") == 0) ||
		    (strcmp(file, "./lib64") == 0)) {
			free(file);
			continue;
		}
		/*
		 * Obsolete obj found, remove it.
		 */
		if (remove(file) == -1) {
			xbps_set_cb_state(xhp,
			    XBPS_STATE_REMOVE_FILE_OBSOLETE_FAIL,
			    errno, pkgname, version,
			    "%s: failed to remove obsolete entry `%s': %s",
			    pkgver, file, strerror(errno));
			free(file);
			continue;
		}
		xbps_set_cb_state(xhp,
		    XBPS_STATE_REMOVE_FILE_OBSOLETE,
		    0, pkgname, version,
		    "%s: removed obsolete entry: %s", pkgver, file);
		free(file);
	}
out1:
	if (!dolinks) {
		/*
		 * Now look for obsolete links.
		 */
		dolinks = true;
		array_str = "links";
		if (iter2)
			prop_object_iterator_release(iter2);
		if (iter)
			prop_object_iterator_release(iter);
		iter2 = iter = NULL;
		goto again;
	}
	if (!dodirs) {
		/*
		 * Look for obsolete dirs.
		 */
		dodirs = true;
		array_str = "dirs";
		if (iter2)
			prop_object_iterator_release(iter2);
		if (iter)
			prop_object_iterator_release(iter);
		iter2 = iter = NULL;
		goto again;
	}

out:
	if (iter2)
		prop_object_iterator_release(iter2);
	if (iter)
		prop_object_iterator_release(iter);

	return rv;
}
