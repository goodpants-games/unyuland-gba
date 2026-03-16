#include <string.h>
#include <tonc.h>
#include "dialogue.h"

const char *dlg_get_chat_by_name(const char *name)
{
    if (!name) return NULL;
    
    // truncate given string to 16 characters. zero-pad extra characters with
    // NUL bytes if too short.
    char test_str[16];
    memset32(test_str, 0, sizeof(test_str) / 4);

    for (uint i = 0; i < 16; ++i)
    {
        if (!name[i]) break;
        test_str[i] = name[i];
    }
    
    for (int i = 0; i < dlg_get_root()->chat_count; ++i)
    {
        const dlg_chat_header_s *header = dlg_get_chat_headers() + i;
        if (memcmp(header->id, test_str, 16)) continue;

        return dlg_get_chat_data(i);
    }

    return NULL;
}