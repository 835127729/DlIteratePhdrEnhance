//
// Created by chenkaiyi on 2024/7/1.
//

#pragma once

#include <stdint.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

uint32_t gnu_hash(const char *s) {
    uint32_t h = 5381;
    for (unsigned char c = *s; c != '\0'; c = *++s) {
        h = h * 33 + c;
    }
    return h;
}

uint32_t elf_hash(const char *s) {
    const uint8_t *name_bytes = (const uint8_t *) (s);
    uint32_t h = 0, g;

    while (*name_bytes) {
        h = (h << 4) + *name_bytes++;
        g = h & 0xf0000000;
        h ^= g;
        h ^= g >> 24;
    }

    return h;
}

__END_DECLS
