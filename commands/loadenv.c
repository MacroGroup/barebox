// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: © 2007 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix

/**
 * @file
 * @brief loadenv: Restoring a environment
 */

#include <common.h>
#include <getopt.h>
#include <command.h>
#include <envfs.h>
#include <errno.h>
#include <fs.h>
#include <libfile.h>
#include <malloc.h>
#include <globalvar.h>

static int do_loadenv(int argc, char *argv[])
{
	const char *filename = NULL;
	char *dirname;
	unsigned flags = 0;
	int opt, ret;
	int scrub = 0;
	int defaultenv = 0;

	while ((opt = getopt(argc, argv, "nsd")) > 0) {
		switch (opt) {
		case 'n':
			flags |= ENV_FLAG_NO_OVERWRITE;
			break;
		case 's':
			scrub = 1;
			break;
		case 'd':
			defaultenv = 1;
			break;
		default:
			return COMMAND_ERROR_USAGE;
		}
	}

	if (argc - optind < 2)
		dirname = "/env";
	else
		dirname = argv[optind + 1];

	if (argc - optind < 1) {
		filename = default_environment_path_get();
		if (!filename) {
			printf("Default environment path not set\n");
			return 1;
		}
	} else {
		/*
		 * /dev/defaultenv use to contain the defaultenvironment.
		 * we do not have this file anymore, but maintain compatibility
		 * to the 'loadenv -s /dev/defaultenv' command to restore the
		 * default environment for some time.
		 */
		if (!strcmp(argv[optind], "/dev/defaultenv"))
			defaultenv = 1;
		else
			filename = argv[optind];
	}

	if (scrub) {
		int ret;

		ret = unlink_recursive(dirname, NULL);
		if (ret && ret != -ENOENT) {
			eprintf("cannot remove %s: %pe\n", dirname,
					ERR_PTR(ret));
			return 1;
		}

		ret = mkdir(dirname, 0);
		if (ret) {
			eprintf("cannot create %s: %pe\n", dirname,
					ERR_PTR(ret));
			return ret;
		}
	}

	printf("loading environment from %s\n", defaultenv ? "defaultenv" : filename);

	if (defaultenv)
		ret = defaultenv_load(dirname, flags);
	else
		ret = envfs_load(filename, dirname, flags);

	nvvar_load();

	return ret;
}

BAREBOX_CMD_HELP_START(loadenv)
BAREBOX_CMD_HELP_TEXT("Load environment from files in ENVFS (default /dev/env0) in")
BAREBOX_CMD_HELP_TEXT("DIRECTORY (default /env)")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("Options:")
BAREBOX_CMD_HELP_OPT("-n", "do not overwrite existing files")
BAREBOX_CMD_HELP_OPT("-s", "scrub old environment")
BAREBOX_CMD_HELP_OPT("-d", "load default environment")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(loadenv)
	.cmd		= do_loadenv,
	BAREBOX_CMD_DESC("load environment from ENVFS")
	BAREBOX_CMD_OPTS("[-nsd] [ENVFS] [DIRECTORY]")
	BAREBOX_CMD_GROUP(CMD_GRP_ENV)
	BAREBOX_CMD_HELP(cmd_loadenv_help)
BAREBOX_CMD_END
