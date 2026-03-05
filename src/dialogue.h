#ifndef DIALOGUE_H
#define DIALOGUE_H

#include <tonc_types.h>
#include <dlg_bin.h>

typedef struct dlg_chat_header
{
    char id[16];
    u16 offset;
} dlg_chat_header_s;

typedef struct dlg_root
{
    u16 chat_count;
    dlg_chat_header_s chat_header0;
} dlg_root_s;

static inline const dlg_root_s* dlg_get_root(void)
{
    return (const dlg_root_s *)dlg_bin;
}

static inline const dlg_chat_header_s* dlg_get_chat_headers(void)
{
    return &(dlg_get_root()->chat_header0);
}

static inline const char* dlg_get_chat_data(int idx)
{
    const dlg_chat_header_s *header_root = dlg_get_chat_headers();
    uintptr_t base = (uintptr_t)(header_root + dlg_get_root()->chat_count);
    return (const char *)(base + header_root[idx].offset);
}

#endif