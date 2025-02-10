//
// Created by chenkaiyi on 2024/5/18.
//

#pragma once

#include <stddef.h>
#include <link.h>
#include <sys/cdefs.h>
#include <sys/auxv.h>

__BEGIN_DECLS

#ifndef __LP64__
#define LINKER_BASENAME "linker"
#define LINKER_PATH "/system/bin/linker"
#else
#define LINKER_BASENAME "linker64"
#define LINKER_PATH "/system/bin/linker64"
#endif
#define VDSO_BASENAME "[vdso]"

typedef int (*dl_iterate_phdr_cb_t)(struct dl_phdr_info *, size_t, void *);

extern __attribute((weak)) int dl_iterate_phdr(int (*)(struct dl_phdr_info *, size_t, void *), void *);

extern __attribute((weak)) unsigned long int getauxval(unsigned long int __type);

uintptr_t linkerBase();

void lockLinker();

void unLockLinker();

__END_DECLS