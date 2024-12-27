#include <sys/types.h>
#include <link.h>

#pragma once

#ifndef __LP64__
#define LINKER_BASENAME "linker"
#define LINKER_PATH "/system/bin/linker"
#else
#define LINKER_BASENAME "linker64"
#define LINKER_PATH "/system/bin/linker64"
#endif
#define VDSO_BASENAME "[vdso]"

#include <sys/cdefs.h>

__BEGIN_DECLS

typedef int (*dl_iterate_phdr_cb_t)(struct dl_phdr_info *, size_t, void *);

int dl_iterate_phdr_by_maps(dl_iterate_phdr_cb_t callback, void *data);

int bypass_dl_iterate_phdr(dl_iterate_phdr_cb_t callback, void *__data);

__END_DECLS