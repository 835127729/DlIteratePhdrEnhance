#include <sys/types.h>
#include <link.h>

#pragma once

#include <sys/cdefs.h>

__BEGIN_DECLS

typedef int (*dl_iterate_phdr_cb_t)(struct dl_phdr_info * info, size_t size, void * data);

int dl_iterate_phdr_by_maps(dl_iterate_phdr_cb_t callback, void *data);

int dl_iterate_phdr_enhance(dl_iterate_phdr_cb_t callback, void *data);

__END_DECLS