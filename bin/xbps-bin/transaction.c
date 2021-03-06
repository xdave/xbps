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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>

#include <xbps_api.h>
#include "compat.h"
#include "defs.h"
#include "../xbps-repo/defs.h"

struct transaction {
	prop_dictionary_t d;
	prop_object_iterator_t iter;
	uint32_t inst_pkgcnt;
	uint32_t up_pkgcnt;
	uint32_t cf_pkgcnt;
	uint32_t rm_pkgcnt;
};

static void
show_missing_deps(prop_array_t a)
{
	size_t i;
	const char *str;

	fprintf(stderr, "xbps-bin: unable to locate some required packages:\n");
	for (i = 0; i < prop_array_count(a); i++) {
		prop_array_get_cstring_nocopy(a, i, &str);
		fprintf(stderr, "  * Missing binary package for: %s\n", str);
	}
}

static void
show_conflicts(prop_array_t a)
{
	size_t i;
	const char *str;

	fprintf(stderr, "xbps-bin: conflicting packages were found:\n");
	for (i = 0; i < prop_array_count(a); i++) {
		prop_array_get_cstring_nocopy(a, i, &str);
		fprintf(stderr, " %s\n", str);
	}
}

static void
show_actions(prop_object_iterator_t iter)
{
	prop_object_t obj;
	const char *repoloc, *trans, *pkgname, *version, *fname, *arch;

	repoloc = trans = pkgname = version = fname = arch = NULL;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &trans);
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "version", &version);
		printf("%s %s %s", pkgname, trans, version);
		prop_dictionary_get_cstring_nocopy(obj, "repository", &repoloc);
		prop_dictionary_get_cstring_nocopy(obj, "filename", &fname);
		prop_dictionary_get_cstring_nocopy(obj, "architecture", &arch);
		if (repoloc && fname && arch)
			printf(" %s %s %s", repoloc, fname, arch);

		printf("\n");
	}
}

static int
show_binpkgs_url(struct xbps_handle *xhp, prop_object_iterator_t iter)
{
	prop_object_t obj;
	const char *repoloc, *trans;
	char *binfile;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &trans);
		if ((strcmp(trans, "remove") == 0) ||
		    (strcmp(trans, "configure") == 0))
			continue;

		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "repository", &repoloc))
			continue;

		/* ignore pkgs from local repositories */
		if (!xbps_check_is_repository_uri_remote(repoloc))
			continue;

		binfile = xbps_path_from_repository_uri(xhp, obj, repoloc);
		if (binfile == NULL)
			return errno;
		/*
		 * If downloaded package is in cachedir, ignore it.
		 */
		if (access(binfile, R_OK) == 0) {
			free(binfile);
			continue;
		}
		printf("%s\n", binfile);
		free(binfile);
	}
	prop_object_iterator_reset(iter);
	return 0;
}

static void
show_package_list(prop_object_iterator_t iter, const char *match, size_t cols)
{
	prop_object_t obj;
	const char *pkgver, *tract;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		if (strcmp(match, tract))
			continue;
		print_package_line(pkgver, cols, false);
	}
	prop_object_iterator_reset(iter);
	print_package_line(NULL, cols, true);
}

static int
show_transaction_sizes(struct transaction *trans, size_t cols)
{
	uint64_t dlsize = 0, instsize = 0, rmsize = 0;
	char size[8];

	/*
	 * Show the list of packages that will be installed.
	 */
	if (prop_dictionary_get_uint32(trans->d, "total-install-pkgs",
	    &trans->inst_pkgcnt)) {
		printf("%u package%s will be installed:\n",
		    trans->inst_pkgcnt, trans->inst_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "install", cols);
		printf("\n");
	}
	if (prop_dictionary_get_uint32(trans->d, "total-update-pkgs",
	    &trans->up_pkgcnt)) {
		printf("%u package%s will be updated:\n",
		    trans->up_pkgcnt, trans->up_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "update", cols);
		printf("\n");
	}
	if (prop_dictionary_get_uint32(trans->d, "total-configure-pkgs",
	    &trans->cf_pkgcnt)) {
		printf("%u package%s will be configured:\n",
		    trans->cf_pkgcnt, trans->cf_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "configure", cols);
		printf("\n");
	}
	if (prop_dictionary_get_uint32(trans->d, "total-remove-pkgs",
	    &trans->rm_pkgcnt)) {
		printf("%u package%s will be removed:\n",
		    trans->rm_pkgcnt, trans->rm_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "remove", cols);
		printf("\n");
	}
	/*
	 * Show total download/installed/removed size for all required packages.
	 */
	printf("\n");
	prop_dictionary_get_uint64(trans->d, "total-download-size", &dlsize);
	if (dlsize > 0) {
		if (xbps_humanize_number(size, (int64_t)dlsize) == -1) {
			xbps_error_printf("xbps-bin: error: humanize_number returns "
			    "%s\n", strerror(errno));
			return -1;
		}
		printf("Total download size:\t%6s\n", size);
	}
	prop_dictionary_get_uint64(trans->d, "total-installed-size",
	    &instsize);
	if (instsize > 0) {
		if (xbps_humanize_number(size, (int64_t)instsize) == -1) {
			xbps_error_printf("xbps-bin: error: humanize_number2 returns "
			    "%s\n", strerror(errno));
			return -1;
		}
		printf("Total installed size:\t%6s\n", size);
	}
	prop_dictionary_get_uint64(trans->d, "total-removed-size", &rmsize);
	if (rmsize > 0) {
		if (xbps_humanize_number(size, (int64_t)rmsize) == -1) {
			xbps_error_printf("xbps-bin: error: humanize_number3 returns "
			    "%s\n", strerror(errno));
			return -1;
		}
		printf("Total freed size:\t%6s\n", size);
	}
	printf("\n");

	return 0;
}

int
dist_upgrade(struct xbps_handle *xhp,
	     size_t cols,
	     bool yes,
	     bool dry_run,
	     bool show_download_pkglist_url)
{
	int rv = 0;

	/*
	 * Update all currently installed packages, aka
	 * "xbps-bin autoupdate".
	 */
	if ((rv = xbps_transaction_update_packages(xhp)) != 0) {
		if (rv == ENOENT) {
			printf("No packages currently registered.\n");
			return 0;
		} else if (rv == EEXIST) {
			printf("All packages are up-to-date.\n");
			return 0;
		} else if (rv == ENOTSUP) {
			xbps_error_printf("xbps-bin: no repositories currently "
			    "registered!\n");
			return -1;
		} else {
			xbps_error_printf("xbps-bin: unexpected error %s\n",
			    strerror(rv));
			return -1;
		}
	}
	return exec_transaction(xhp, cols, yes, dry_run,
	    show_download_pkglist_url);
}

int
remove_pkg_orphans(struct xbps_handle *xhp, size_t cols, bool yes, bool dry_run)
{
	int rv;

	if ((rv = xbps_transaction_autoremove_pkgs(xhp)) != 0) {
		if (rv == ENOENT) {
			printf("No package orphans were found.\n");
			return 0;
		} else {
			printf("Failed to remove package orphans: %s\n",
			    strerror(rv));
			return rv;
		}
	}
	return exec_transaction(xhp, cols, yes, dry_run, false);
}

int
install_new_pkg(struct xbps_handle *xhp, const char *pkg, bool reinstall)
{
	int rv;

	if ((rv = xbps_transaction_install_pkg(xhp, pkg, reinstall)) != 0) {
		if (rv == EEXIST) {
			printf("Package `%s' already installed.\n", pkg);
		} else if (rv == ENOENT) {
			xbps_error_printf("xbps-bin: unable to locate '%s' in "
			    "repository pool.\n", pkg);
		} else if (rv == ENOTSUP) {
			xbps_error_printf("xbps-bin: no repositories  "
			    "currently registered!\n");
		} else {
			xbps_error_printf("xbps-bin: unexpected error: %s\n",
			    strerror(rv));
			rv = -1;
		}
	}
	return rv;
}

int
update_pkg(struct xbps_handle *xhp, const char *pkgname)
{
	int rv;

	rv = xbps_transaction_update_pkg(xhp, pkgname);
	if (rv == EEXIST)
		printf("Package '%s' is up to date.\n", pkgname);
	else if (rv == ENOENT)
		fprintf(stderr, "Package '%s' not found in "
		    "repository pool.\n", pkgname);
	else if (rv == ENODEV)
		printf("Package '%s' not installed.\n", pkgname);
	else if (rv == ENOTSUP)
		xbps_error_printf("xbps-bin: no repositories currently "
		    "registered!\n");
	else if (rv != 0) {
		xbps_error_printf("xbps-bin: unexpected error %s\n",
		    strerror(rv));
		return -1;
	}
	return rv;
}

int
remove_pkg(struct xbps_handle *xhp, const char *pkgname, size_t cols,
		bool recursive)
{
	prop_dictionary_t pkgd;
	prop_array_t reqby;
	const char *pkgver;
	size_t x;
	int rv;

	rv = xbps_transaction_remove_pkg(xhp, pkgname, recursive);
	if (rv == EEXIST) {
		/* pkg has revdeps */
		pkgd = xbps_find_pkg_dict_installed(xhp, pkgname, false);
		prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		reqby = prop_dictionary_get(pkgd, "requiredby");
		printf("WARNING: %s IS REQUIRED BY %u PACKAGE%s:\n\n",
		    pkgver, prop_array_count(reqby),
		    prop_array_count(reqby) > 1 ? "S" : "");
		for (x = 0; x < prop_array_count(reqby); x++) {
			prop_array_get_cstring_nocopy(reqby, x, &pkgver);
			print_package_line(pkgver, cols, false);
		}
		printf("\n\n");
		print_package_line(NULL, cols, true);
		return rv;
	} else if (rv == ENOENT) {
		printf("Package `%s' is not currently installed.\n", pkgname);
		return 0;
	} else if (rv != 0) {
		xbps_error_printf("Failed to queue `%s' for removing: %s\n",
		    pkgname, strerror(rv));
		return rv;
	}

	return 0;
}

int
exec_transaction(struct xbps_handle *xhp,
		 size_t maxcols,
		 bool yes,
		 bool dry_run,
		 bool show_download_urls)
{
	prop_array_t mdeps, cflicts;
	struct transaction *trans;
	int rv = 0;

	trans = calloc(1, sizeof(*trans));
	if (trans == NULL)
		return ENOMEM;

	if ((rv = xbps_transaction_prepare(xhp)) != 0) {
		if (rv == ENODEV) {
			mdeps =
			    prop_dictionary_get(xhp->transd, "missing_deps");
			/* missing packages */
			show_missing_deps(mdeps);
			goto out;
		} else if (rv == EAGAIN) {
			/* conflicts */
			cflicts = prop_dictionary_get(xhp->transd, "conflicts");
			show_conflicts(cflicts);
			goto out;
		}
		xbps_dbg_printf(xhp, "Empty transaction dictionary: %s\n",
		    strerror(errno));
		return rv;
	}
	xbps_dbg_printf(xhp, "Dictionary before transaction happens:\n");
	xbps_dbg_printf_append(xhp, "%s",
	    prop_dictionary_externalize(xhp->transd));

	trans->d = xhp->transd;
	trans->iter = xbps_array_iter_from_dict(xhp->transd, "packages");
	if (trans->iter == NULL) {
		rv = errno;
		xbps_error_printf("xbps-bin: error allocating array mem! (%s)\n",
		    strerror(errno));
		goto out;
	}
	/*
	 * dry-run mode, show what would be done but don't run anything.
	 */
	if (dry_run) {
		show_actions(trans->iter);
		goto out;
	}
	/*
	 * Only show URLs to download binary packages.
	 */
	if (show_download_urls) {
		rv = show_binpkgs_url(xhp, trans->iter);
		goto out;
	}
	/*
	 * Show download/installed size for the transaction.
	 */
	if ((rv = show_transaction_sizes(trans, maxcols)) != 0)
		goto out;
	/*
	 * Ask interactively (if -y not set).
	 */
	if (!yes && !yesno("Do you want to continue?")) {
		printf("Aborting!\n");
		goto out;
	}
	/*
	 * It's time to run the transaction!
	 */
	if ((rv = xbps_transaction_commit(xhp)) == 0) {
		printf("\nxbps-bin: %u installed, %u updated, "
		    "%u configured, %u removed.\n", trans->inst_pkgcnt,
		    trans->up_pkgcnt, trans->cf_pkgcnt + trans->inst_pkgcnt,
		    trans->rm_pkgcnt);
	}
out:
	if (trans->iter)
		prop_object_iterator_release(trans->iter);
	if (trans)
		free(trans);

	return rv;
}
