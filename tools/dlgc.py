#!/usr/bin/env python3

"""
struct dlg_root
{
    u32 chat_count;
    struct {
        char id[16];
        u32 pointer; // relative to root
    } chat_index[?];
}
"""