/*
 * siphash24.h
 * 
 * Forked from systemd/src/shared/siphash24.h on March 26, 2015
 * 
 * All modifications to the original source file are Copyright (C) 2015 Jude Nelson <judecn@gmail.com>
 */

#ifndef _LIBUDEV_COMPAT_SIPHASH24_H_
#define _LIBUDEV_COMPAT_SIPHASH24_H_

#include <inttypes.h>
#include <sys/types.h>

void siphash24 (uint8_t out[8], const void *in, size_t inlen,
		const uint8_t k[16]);

#endif
