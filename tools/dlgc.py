#!/usr/bin/env python3
import argparse
import ioutil
import json
import sys
import struct
from typing import BinaryIO

MAX_COLS = 20
MAX_ROWS = 6

"""
struct dlg_root
{
    u16 chat_count;
    struct
    {
        char id[16];
        u16 offset;
    } chat_data[chat_count];
    dlg_chat chats[chat_count];
}

struct dlg_chat
{
    // each page is NUL-terminated.
    char pages[?][?]
}
"""

def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


def wrap_text(text: str, max_cols: int):
    out: list[str] = []
    
    write_nl = False
    for line in text.splitlines():
        if write_nl:
            out.append("\n")
        write_nl = True
        
        while len(line) > max_cols:
            ch = max_cols
            while not line[ch].isspace():
                ch = ch - 1
            
            # assert len(text[:ch]) <= max_cols
            out.append(line[:ch] + '\n')
            line = line[ch:].lstrip()
    
        # assert len(text) <= max_cols
        out.append(line)
    
    return out


def process(in_json: dict, out_path: str):
    chat_data_list: bytearray = bytearray()
    chat_data_offsets: list[int] = []
    cur_chat_data_offset = 0
    success = True

    for chat in in_json:
        chat_data = bytearray()
        pages = chat['pages']

        for page_index in range(0, len(pages)):
            text = pages[page_index]

            lines = wrap_text(text, MAX_COLS)
            if len(lines) > MAX_ROWS:
                eprint(f"error: page {(page_index + 1)} of {(chat['id'])}, with a line count of {(len(lines))}, exceeds the max length of {MAX_ROWS} lines")
                success = False
            
            for line in lines:
                chat_data += bytes(line, 'ascii')
            
            chat_data.append(0x0C)
        
        chat_data.append(0)

        chat_data_offsets.append(cur_chat_data_offset)
        cur_chat_data_offset += len(chat_data)
        chat_data_list += chat_data
    
    if not success: exit(1)
    
    out_data = bytearray()
    out_data += struct.pack('<H', len(in_json))

    for i in range(0, len(in_json)):
        chat = in_json[i]

        out_data += struct.pack('<16sH', bytes(chat['id'], 'ascii'),
                                chat_data_offsets[i])
    
    out_data += chat_data_list
    
    if out_path:
        with ioutil.open_output(out_path, binary=True) as out_file:
            out_file.write(out_data)


def main() -> None:
    parser = argparse.ArgumentParser(prog='dlgx')
    parser.add_argument('input', help="path to input .json file. pass - to read from stdin.")
    parser.add_argument('output', help="output bin file. pass - to write to stdout.")

    args = parser.parse_args()

    with ioutil.open_input(args.input, binary=True) as in_file:
        in_json = json.load(in_file)

    process(in_json, args.output)


if __name__ == '__main__':
    main()