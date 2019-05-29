/*
 * Copyright 2016,2019 Alexander Peslyak
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

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "yespower/sha256.h"

#include "random.h"

static int random_fd = -1;

int random_init(void)
{
	random_fd = open("/dev/urandom", O_RDONLY);
	if (random_fd < 0) {
		perror("open: /dev/urandom");
		return -1;
	}

	return 0;
}

static ssize_t read_loop(int fd, void *buffer, ssize_t count)
{
	ssize_t offset, block;

	offset = 0;
	while (count > 0) {
		block = read(fd, (char *)buffer + offset, count);

		if (block < 0)
			return block;
		if (!block)
			return offset;

		offset += block;
		count -= block;
	}

	return offset;
}

uint8_t *random_get(void *extra, size_t extralen)
{
	static __thread uint8_t pool[32];
	static __thread bool pool_initialized = false;

	if (!pool_initialized) {
#if 0
/* Insist on proper pool initialization */
		if (random_fd < 0 || read_loop(random_fd, pool, sizeof(pool)) != sizeof(pool))
			return NULL;
#else
/* Try our best, but accept what we get */
		if (random_fd >= 0)
			read_loop(random_fd, pool, sizeof(pool));
#endif
		pool_initialized = true;
	}

	static __thread uint8_t out[32];
	SHA256_CTX ctx;

	SHA256_Init(&ctx);
	SHA256_Update(&ctx, "OUT", 4);
#if 0
/*
 * Ensure variety between threads, but OTOH this is an ASLR leak if the pool
 * is weakly initialized.
 */
	void *stackptr = &ctx;
	SHA256_Update(&ctx, stackptr, sizeof(stackptr));
#endif
	SHA256_Update(&ctx, pool, sizeof(pool));
	SHA256_Update(&ctx, extra, extralen);
	SHA256_Final(out, &ctx);

	SHA256_Init(&ctx);
	SHA256_Update(&ctx, "UPD", 4);
	SHA256_Update(&ctx, pool, sizeof(pool));
	SHA256_Update(&ctx, extra, extralen);
	SHA256_Final(pool, &ctx);

	return out;
}
