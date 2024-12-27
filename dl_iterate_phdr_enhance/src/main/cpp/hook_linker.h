//
// Created by chenkaiyi on 2024/5/18.
//

#pragma once

#include <stddef.h>
#include <link.h>

#ifndef __LP64__
#define LINKER_BASENAME "linker"
#define LINKER_PATH "/system/bin/linker"
#else
#define LINKER_BASENAME "linker64"
#define LINKER_PATH "/system/bin/linker64"
#endif

#include <sys/cdefs.h>

__BEGIN_DECLS

typedef int (*dl_iterate_phdr_cb_t)(struct dl_phdr_info *, size_t, void *);

extern __attribute((weak)) int
dl_iterate_phdr(int (*)(struct dl_phdr_info *, size_t, void *), void *);

void lockLinker();

void unLockLinker();

__END_DECLS