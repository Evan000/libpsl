/*
 * Copyright(c) 2017-2018 Tim Ruehsen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * This file is part of libpsl.
 */

#include <config.h>

#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdlib.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "fuzzer.h"

#if defined (TEST_RUN) && defined (HAVE_FMEMOPEN)

#include <dirent.h>
#ifdef HAVE_ALLOCA_H
#  include <alloca.h>
#endif

static void test_all_from(const char *dirname)
{
	DIR *dirp;
	struct dirent *dp;

	if ((dirp = opendir(dirname))) {
		while ((dp = readdir(dirp))) {
			size_t fnamesize;
			char *fname;
			int fd;
			struct stat st;
			uint8_t *data;
			ssize_t n;

			if (*dp->d_name == '.') continue;

			fnamesize = strlen(dirname) + strlen(dp->d_name) + 2;
			fname = alloca(fnamesize);
			snprintf(fname, fnamesize, "%s/%s", dirname, dp->d_name);

			if ((fd = open(fname, O_RDONLY)) == -1) {
				fprintf(stderr, "Failed to open %s (%d)\n", fname, errno);
				continue;
			}

			if (fstat(fd, &st) != 0) {
				fprintf(stderr, "Failed to stat %d (%d)\n", fd, errno);
				close(fd);
				continue;
			}

			data = malloc(st.st_size);
			if ((n = read(fd, data, st.st_size)) == st.st_size) {
				printf("testing %u bytes from '%s'\n", (int) st.st_size, fname);
				LLVMFuzzerTestOneInput(data, st.st_size);
			} else
				fprintf(stderr, "Failed to read %d bytes from %s (%d), got %d\n", (int) st.st_size, fname, errno, (int) n);

			free(data);
			close(fd);
		}
		closedir(dirp);
	}
}

int main(int argc, char **argv)
{
	const char *target;
	size_t corporadirsize = sizeof(SRCDIR) + 1 + strlen(argv[0]) + 8;
	char *corporadir = alloca(corporadirsize);

	/* if VALGRIND testing is enabled, we have to call ourselves with valgrind checking */
	if (argc == 1) {
		const char *valgrind = getenv("TESTS_VALGRIND");

		if (valgrind && *valgrind) {
			size_t cmdsize = strlen(valgrind) + strlen(argv[0]) + 32;
			char *cmd = alloca(cmdsize);

			snprintf(cmd, cmdsize, "TESTS_VALGRIND="" %s %s", valgrind, argv[0]);
			return system(cmd) != 0;
		}
	}

	target = strrchr(argv[0], '/');
	target = target ? target + 1 : argv[0];

	snprintf(corporadir, corporadirsize, SRCDIR "/%s.in", target);

	test_all_from(corporadir);

	snprintf(corporadir, corporadirsize, SRCDIR "/%s.repro", target);

	test_all_from(corporadir);

	return 0;
}

#else /* TEST_RUN && HAVE_FMEMOPEN */

#ifndef __AFL_LOOP
static int __AFL_LOOP(int n)
{
	static int first = 1;

	if (first) {
		first = 0;
		return 1;
	}

	return 0;
}
#endif

int main(int argc, char **argv)
{
#ifdef HAVE_FMEMOPEN
	int ret;
	unsigned char buf[64 * 1024];

	while (__AFL_LOOP(10000)) { /* only works with afl-clang-fast */
		ret = fread(buf, 1, sizeof(buf), stdin);
		if (ret < 0)
			return 0;

		LLVMFuzzerTestOneInput(buf, ret);
	}

	return 0;
#else
	exit (77);
#endif
}

#endif /* TEST_RUN && HAVE_FMEMOPEN*/
