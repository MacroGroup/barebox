// SPDX-License-Identifier: GPL-2.0+
/* SPDX-FileCopyrightText: Alexander Shiyan <shc_work@mail.ru> */

#define pr_fmt(fmt)     "extlinux: " fmt

#include <boot.h>
#include <bootm.h>
#include <bootscan.h>
#include <common.h>
#include <environment.h>
#include <fs.h>
#include <globalvar.h>
#include <libfile.h>
#include <libgen.h>
#include <malloc.h>
#include <string.h>

struct extlinux_entry {
	struct bootentry entry;
	char *rootpath;
	char *label;
	char *kernel;
	char *initrd;
	char *fdtdir;
	char *fdt;
	char *append;
};

static int extlinux_boot(struct bootentry *be, int verbose, int dryrun)
{
	struct extlinux_entry *e =
		container_of(be, struct extlinux_entry, entry);
	char *kernel_abs, *initrd_abs = NULL, *fdt_abs = NULL;
	struct bootm_data data = {};
	int ret;

	bootm_data_init_defaults(&data);

	data.dryrun = max_t(int, dryrun, data.dryrun);
	data.verbose = max(verbose, data.verbose);
	data.appendroot = true;

	kernel_abs = basprintf("%s/%s", e->rootpath, e->kernel);
	data.os_file = kernel_abs;

	if (e->initrd) {
		initrd_abs = basprintf("%s/%s", e->rootpath, e->initrd);
		data.initrd_file = initrd_abs;
	}

	if (e->fdt) {
		char *fdtdir = e->fdtdir ? : e->rootpath;

		fdt_abs = basprintf("%s/%s", fdtdir, e->fdt);
		data.oftree_file = fdt_abs;
	}

	if (e->append)
		globalvar_add_simple("linux.bootargs.dyn.extlinux", e->append);

	pr_info("Booting extlinux label '%s'\n", e->label);

	ret = bootm_boot(&data);
	if (ret)
		pr_err("bootm failed: %s\n", strerror(-ret));

	free(kernel_abs);
	free(initrd_abs);
	free(fdt_abs);

	return ret;
}

static void extlinux_entry_free(struct bootentry *be)
{
	struct extlinux_entry *e =
		container_of(be, struct extlinux_entry, entry);

	free(e->rootpath);
	free(e->label);
	free(e->kernel);
	free(e->initrd);
	free(e->fdtdir);
	free(e->fdt);
	free(e->append);
	free(e);
}

static char *remove_param(const char *params, const char *param)
{
	char *result, *dst;
	const char *src;

	result = xmalloc(strlen(params) + 1);

	src = params;
	dst = result;

	while (*src) {
		if (!strncasecmp(src, param, strlen(param))) {
			while (*src && *src != ' ')
				src++;

			src = skip_spaces(src);

			continue;
		}

		*dst++ = *src++;
	}

	*dst = '\0';

	return result;
}

static struct extlinux_entry *parse_extlinux_conf(const char *abspath,
						  const char *rootpath)
{
	char *buf, *bufptr, *line, *p, *default_label = NULL;
	struct extlinux_entry *entry = NULL;

	bufptr = read_file(abspath, NULL);
	if (!bufptr)
		return ERR_PTR(-errno);

	for (p = bufptr; *p; p++)
		if (*p == '\r')
			*p = '\n';

	buf = bufptr;
	while ((line = strsep(&buf, "\n")) != NULL) {
		char *key, *val;

		line = skip_spaces(line);

		if (*line == '#' || *line == '\0')
			continue;

		key = line;
		val = strchr(line, ' ');
		if (!val)
			val = strchr(line, '\t');
		if (val) {
			*val++ = '\0';
			val = skip_spaces(val);
		} else
			continue;

		if (!default_label) {
			if (!strcasecmp(key, "DEFAULT"))
				default_label = xstrdup(val);

			continue;
		}

		if (!strcasecmp(key, "LABEL")) {
			if (!strcmp(val, default_label)) {
				entry = xzalloc(sizeof(*entry));
				entry->label = xstrdup(val);
				entry->rootpath = dirname(xstrdup(abspath));
			} else if (entry)
				break;
			continue;
		}

		/*
		 * The same rootfs image may be launched from eMMC or SD card.
		 * Remove any hardcoded root= parameter from "append" to avoid
		 * conflicts, then let barebox automatically add the correct
		 * root= (via appendroot) based on the boot device.
		 */
		if (entry) {
			if (!strcasecmp(key, "KERNEL"))
				entry->kernel = xstrdup(val);
			else if (!strcasecmp(key, "INITRD"))
				entry->initrd = xstrdup(val);
			else if (!strcasecmp(key, "FDTDIR"))
				entry->fdtdir = xstrdup(val);
			else if (!strcasecmp(key, "FDT"))
				entry->fdt = xstrdup(val);
			else if (!strcasecmp(key, "APPEND"))
				entry->append = remove_param(val, "ROOT=");
			else
				pr_debug("Unhandled key: %s\n", key);
		}
	}

	free(default_label);
	free(bufptr);

	if (!entry || !entry->kernel) {
		if (entry)
			extlinux_entry_free(&entry->entry);
		return ERR_PTR(-EINVAL);
	}

	return entry;
}

static int extlinux_scan_file(struct bootscanner *scanner,
			      struct bootentries *bootentries,
			      const char *configname)
{
	struct extlinux_entry *e;
	const char *rootpath;

	if (!strends(configname, "extlinux.conf"))
		return 0;

	rootpath = get_mounted_path(configname);
	if (IS_ERR(rootpath))
		return PTR_ERR(rootpath);

	e = parse_extlinux_conf(configname, rootpath);
	if (IS_ERR(e))
		return PTR_ERR(e);

	e->entry.boot = extlinux_boot;
	e->entry.release = extlinux_entry_free;
	e->entry.path = xstrdup(configname);
	e->entry.title = basprintf("extlinux: %s", e->label);
	e->entry.description = basprintf("extlinux entry \'%s\" on %s",
					 e->label, rootpath);
	e->entry.me.type = MENU_ENTRY_NORMAL;

	bootentries_add_entry(bootentries, &e->entry);

	return 1;
}

static int extlinux_scan_directory(struct bootscanner *scanner,
				   struct bootentries *bootentries,
				   const char *rootpath)
{
	char *path;
	struct stat s;
	int ret;

	path = basprintf("%s/boot/extlinux/extlinux.conf", rootpath);

	ret = stat(path, &s);
	if (!ret && S_ISREG(s.st_mode))
		ret = extlinux_scan_file(scanner, bootentries, path);

	free(path);

	return ret < 1 ? ret : 1;
}

static struct bootscanner extlinux_scanner = {
	.name = "extlinux",
	.scan_file = extlinux_scan_file,
	.scan_directory = extlinux_scan_directory,
};

static int extlinux_generate(struct bootentries *bootentries, const char *name)
{
	return bootentry_scan_generate(&extlinux_scanner, bootentries, name);
}

static struct bootentry_provider extlinux_provider = {
	.name = "extlinux",
	.generate = extlinux_generate,
};

static int extlinux_init(void)
{
	return bootentry_register_provider(&extlinux_provider);
}
device_initcall(extlinux_init);
