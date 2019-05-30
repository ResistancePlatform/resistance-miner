/*
 * Copyright 2016,2018 Alexander Peslyak
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(__linux__) && (defined(__x86_64__) || defined(__i386__))

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "cpuinfo.h"

cpuinfo_t cpuinfo;

#define PROC_LINE_MAX 1024

int cpuinfo_init(void)
{
	const char *filename = "/proc/cpuinfo";
	FILE *f = fopen(filename, "r");
	if (!f) {
		fprintf(stderr, "fopen: %s: %s\n", filename, strerror(errno));
		return -1;
	}

	const char *errmsg = NULL;
	int retval = -1;
	uint64_t linenum = 0;
	char line[PROC_LINE_MAX];
	char name[PROC_LINE_MAX];
	uint32_t value;

	memset(&cpuinfo, -1, sizeof(cpuinfo));
	int32_t i = -1;

	while (fgets(line, sizeof(line), f)) {
		linenum++;
		char *p = line + strlen(line);
		if (p > line)
			p--;
		if (p >= line + sizeof(line) - 2) {
			errmsg = "line too long";
			goto out;
		} else if (*p != '\n') {
			errmsg = "no linefeed";
			goto out;
		}

		if (sscanf(line, "%[^\t]\t: %" SCNu32 "\n", name, &value) != 2)
			continue;

		if (!strcmp(name, "processor")) {
			if (value != ++i) {
				errmsg = "unexpected logical processor number";
				goto out;
			}
			if (i >= CPUINFO_LOGICAL_MAX) {
				errmsg = "too many logical processors";
				goto out;
			}
		}
		if (!strcmp(name, "physical id")) {
			if (i == -1) {
				errmsg = "physical id without processor";
				goto out;
			}
			if (value >= CPUINFO_CHIP_MAX) {
				errmsg = "physical id too large";
				goto out;
			}
			if (cpuinfo.log2phy[i].chip != (uint32_t)-1) {
				errmsg = "duplicate physical id for processor";
				goto out;
			}
			cpuinfo.log2phy[i].chip = value;
		}
		if (!strcmp(name, "core id")) {
			if (i == -1) {
				errmsg = "core id without processor";
				goto out;
			}
			if (value >= CPUINFO_CORE_MAX) {
				errmsg = "core id too large";
				goto out;
			}
			if (cpuinfo.log2phy[i].core != (uint32_t)-1) {
				errmsg = "duplicate core id for processor";
				goto out;
			}
			cpuinfo.log2phy[i].core = value;
		}
	}

	if (i == -1) {
		errmsg = "no logical processors found";
		goto out;
	}

	cpuinfo.logical = ++i;

	uint32_t j;
	for (i = j = 0; i < cpuinfo.logical; i++) {
		if (cpuinfo.log2phy[i].chip == (uint32_t)-1) {
			errmsg = "no physical id for a logical processor";
			goto out;
		}
		if (cpuinfo.log2phy[i].core == (uint32_t)-1) {
			errmsg = "no core id for a logical processor";
			goto out;
		}
		uint32_t *p2l = &cpuinfo.phy2log
		    [cpuinfo.log2phy[i].chip][cpuinfo.log2phy[i].core];
		if (*p2l == (uint32_t)-1) {
			*p2l = i;
			cpuinfo.log2phy[i].seq = j++;
		} else {
			cpuinfo.log2phy[i].seq = cpuinfo.log2phy[*p2l].seq;
		}
	}

	cpuinfo.physical = j;

	if (ferror(f))
		perror("fgets");
	else
		retval = 0;

out:
	if (errmsg)
		fprintf(stderr, "Error: %s line %" PRIu64 ": %s\n",
		    filename, linenum, errmsg);
	if (fclose(f))
		perror("fclose");

	if (!retval) {
		printf("Found %" PRIu32 " logical processors across %" PRIu32
		    " physical cores\n", cpuinfo.logical, cpuinfo.physical);
	}

	return retval;
}

#endif
