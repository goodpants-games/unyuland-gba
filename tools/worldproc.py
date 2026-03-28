#!/usr/bin/env python3
import sys
import os.path as path
import argparse
import json
import typing
import struct
import ioutil

ROOM_SCREEN_WIDTH = 15
ROOM_SCREEN_HEIGHT = 11
WORLD_GRID_WIDTH = ROOM_SCREEN_WIDTH * 8
WORLD_GRID_HEIGHT = ROOM_SCREEN_HEIGHT * 8
MAP_SCREEN_SUBDIV = 2
MAP_SCREEN_MARGIN_X = 2
MAP_SCREEN_MARGIN_Y = 2

ADJBIT_R  = 0x1
ADJBIT_U  = 0x2
ADJBIT_L  = 0x4
ADJBIT_D  = 0x8
ADJBIT_TR = 0x10
ADJBIT_TL = 0x20
ADJBIT_BL = 0x40
ADJBIT_BR = 0x80

g_errors: list[Exception] = []

class RoomData:
    def __init__(self: typing.Self, name: str, x: float, y: float, w: float, h: float):
        if w % WORLD_GRID_WIDTH != 0 or h % WORLD_GRID_HEIGHT != 0:
            g_errors.append(
                RuntimeError(f"room '{name}' size is not grid-aligned"))
            return

        self.name = name
        self.x = x
        self.y = y
        self.w = w
        self.h = h

class Array2D:
    def __init__(self: typing.Self, w: int, h: int, data: list):
        self.data = data
        self.w = w
        self.h = h
    
    def create(w: int, h: int, init):
        return Array2D(w, h, [init] * (w * h))
    
    def is_in_bounds(self: typing.Self, x: int, y: int) -> bool:
        return x >= 0 and y >= 0 and x < self.w and y < self.h
    
    def get(self: typing.Self, x: int, y: int, default=None):
        if not self.is_in_bounds(x, y): return default
        return self.data[y * self.w + x]
    
    def set(self: typing.Self, x: int, y: int, v):
        if not self.is_in_bounds(x, y): return None
        self.data[y * self.w + x] = v


def int_ceil_div(x: int, y: int) -> int:
    return (x + y - 1) // y


def abort_errors():
    if len(g_errors) == 1:
        raise g_errors[0]
    else:
        err_text = ""
        for err in g_errors:
            err_text += "\n\t" + str(err)
        raise RuntimeError("multiple errors occurred: " + err_text)


def worldproc(world_path: str, room_list: list[str],
              out_json: typing.TextIO|None = None,
              out_bin: typing.BinaryIO|None = None,
              out_cp: str|None = None,
              out_automap: str|None = None):
    if len(room_list) >= 255:
        raise RuntimeError('exceeded max room count of 255')

    with open(world_path, 'r') as world_file:
        world_data = json.load(world_file)

    # get dictionary of rooms
    rooms: dict[str, RoomData] = {}
    for map_json in world_data['maps']:
        name, _ = path.splitext(map_json['fileName'])
        rooms[name] = RoomData(name, map_json['x'], map_json['y'],
                               map_json['width'], map_json['height'])
    
    # get bounding box of world
    min_x =  0x7FFFFFFF
    min_y =  0x7FFFFFFF
    max_x = -0x80000000
    max_y = -0x80000000

    for name in room_list:
        room = rooms[name]
        min_x = min(min_x, room.x)
        max_x = max(max_x, room.x + room.w)
        min_y = min(min_y, room.y)
        max_y = max(max_y, room.y + room.h)
    
    bounds_w = max_x - min_x
    bounds_h = max_y - min_y

    # adjust room positions to be relative to bounding box
    for name in room_list:
        room = rooms[name]
        room.x -= min_x
        room.y -= min_y

        if room.x % WORLD_GRID_WIDTH != 0 or room.y % WORLD_GRID_HEIGHT != 0:
            g_errors.append(
                RuntimeError((f"room '{name}' position is not grid-aligned")))
    
    if g_errors:
        abort_errors()

    # build room matrix
    matrix_w = bounds_w // WORLD_GRID_WIDTH
    matrix_h = bounds_h // WORLD_GRID_HEIGHT
    if matrix_w > 0xFFFF or matrix_h > 0xFFFF:
        raise RuntimeError(
            f"world size ({matrix_w}x{matrix_h} screens) is too large!")
    
    matrix = bytearray(matrix_w * matrix_h)

    for i in range(0, len(room_list)):
        name = room_list[i]
        room = rooms[name]

        min_x = room.x // WORLD_GRID_WIDTH
        min_y = room.y // WORLD_GRID_HEIGHT
        max_x = (room.x + room.w) // WORLD_GRID_WIDTH
        max_y = (room.y + room.h) // WORLD_GRID_HEIGHT
        for y in range(min_y, max_y):
            for x in range(min_x, max_x):
                matrix[y * matrix_w + x] = i + 1

    # write room metadata to json file
    if out_json:
        out_json_root = {
            "rooms": {},
            "list": room_list,
        }

        for i in range(0, len(room_list)):
            name = room_list[i]
            room = rooms[name]

            out_json_root['rooms'][name] = {
                "index": i, 
                "x": room.x // WORLD_GRID_WIDTH,
                "y": room.y // WORLD_GRID_HEIGHT,
            }

        json.dump(out_json_root, out_json, indent='  ')

    if out_bin:
        out_bin.write(struct.pack('<HH', matrix_w, matrix_h))
        out_bin.write(matrix)

    if out_cp:
        generate_c_source(room_list, rooms, matrix_w, matrix_h, matrix, out_cp)
    
    if out_automap:
        generate_automap(rooms, matrix_w, matrix_h, world_path, out_automap)


def generate_c_source(rooms: list[str], room_data: dict[str, RoomData],
                      mat_w: int, mat_h: int, matrix: bytes, fc_path: str):
    fh_path = path.splitext(fc_path)[0] + '.h'

    with open(fh_path, 'w') as fh:
        room_count = len(rooms)

        fh.write(f"""#ifndef WORLD_H
#define WORLD_H
#include <tonc_types.h>
#include <map_data.h>

#define WORLD_MATRIX_GRID_WIDTH 15
#define WORLD_MATRIX_GRID_HEIGHT 11
#define WORLD_MATRIX_WIDTH {mat_w}
#define WORLD_MATRIX_HEIGHT {mat_h}
#define WORLD_ROOM_COUNT {room_count}

typedef struct world_room
{{
    u8 x, y;
    const map_header_s *map;
}}
world_room_s;

extern const world_room_s world_rooms[WORLD_ROOM_COUNT];
extern const u8 world_matrix[WORLD_MATRIX_HEIGHT][WORLD_MATRIX_WIDTH];

#endif""")
    
    with open(fc_path, 'w') as fc:
        fc.write("#include \"world.h\"\n\n")

        for name in rooms:
            fc.write("#include <")
            fc.write(name)
            fc.write("_map.h>\n")
        
        fc.write(f"""
const world_room_s world_rooms[WORLD_ROOM_COUNT] = {{""")
        
        for name in rooms:
            data = room_data[name]
            room_x = data.x // WORLD_GRID_WIDTH
            room_y = data.y // WORLD_GRID_HEIGHT

            fc.write(f"\n    {{ .x = {room_x}, .y = {room_y}, ")
            fc.write(".map = (const map_header_s *)")
            fc.write(name)
            fc.write("_map },")
        
        fc.write("\n};\n\n")

        fc.write(f"const unsigned char world_matrix[WORLD_MATRIX_HEIGHT][WORLD_MATRIX_WIDTH] = {{")

        for y in range(0, mat_h):
            fc.write("{")
            for x in range(0, mat_w):
                byte = matrix[y * mat_w + x]
                fc.write(str(byte))
                fc.write(",")
            fc.write("},")
        
        fc.write("};\n")


def calc_adjacency(mat: Array2D, x: int, y: int):
    adj = 0

    if not mat.get(x+1, y  , False): adj |= ADJBIT_R
    if not mat.get(x  , y-1, False): adj |= ADJBIT_U
    if not mat.get(x-1, y  , False): adj |= ADJBIT_L
    if not mat.get(x  , y+1, False): adj |= ADJBIT_D

    if not mat.get(x+1, y-1, False): adj |= ADJBIT_TR
    if not mat.get(x-1, y-1, False): adj |= ADJBIT_TL
    if not mat.get(x-1, y+1, False): adj |= ADJBIT_BL
    if not mat.get(x+1, y+1, False): adj |= ADJBIT_BR

    # change meaning of corner flags. it should only mean inner corners, not
    # outer corners
    if (adj & (ADJBIT_R | ADJBIT_D)) != 0:
        adj &= ~ADJBIT_BR
    
    if (adj & (ADJBIT_R | ADJBIT_U)) != 0:
        adj &= ~ADJBIT_TR
    
    if (adj & (ADJBIT_L | ADJBIT_U)) != 0:
        adj &= ~ADJBIT_TL
    
    if (adj & (ADJBIT_L | ADJBIT_D)) != 0:
        adj &= ~ADJBIT_BL

    return adj


def generate_automap(rooms: dict[str, RoomData], matrix_w: int, matrix_h: int,
                     world_path: str, out_path: str):
    matrix_w *= MAP_SCREEN_SUBDIV
    matrix_h *= MAP_SCREEN_SUBDIV

    tex_lookup: list[int] = [4]
    i = 5
    for flags in range(1, 0x100):
        if (  (flags & ADJBIT_R)!=0 and (flags & (ADJBIT_BR | ADJBIT_TR))!=0
           or (flags & ADJBIT_U)!=0 and (flags & (ADJBIT_TR | ADJBIT_TL))!=0
           or (flags & ADJBIT_L)!=0 and (flags & (ADJBIT_TL | ADJBIT_BL))!=0
           or (flags & ADJBIT_D)!=0 and (flags & (ADJBIT_BL | ADJBIT_BR))!=0):
            tex_lookup.append(0)
            continue

        tex_lookup.append(i)
        i += 1

    matrix: list[int] = []
    for i in range(0, matrix_w * matrix_h):
        matrix.append(0xFF)
    
    for room in rooms.values():
        omatrix = calc_room_automap_occupancy(room.name)

        start_x = room.x // WORLD_GRID_WIDTH * MAP_SCREEN_SUBDIV
        start_y = room.y // WORLD_GRID_HEIGHT * MAP_SCREEN_SUBDIV

        for ly in range(0, room.h // WORLD_GRID_HEIGHT * MAP_SCREEN_SUBDIV):
            gy = start_y + ly
            for lx in range(0, room.w // WORLD_GRID_WIDTH * MAP_SCREEN_SUBDIV):
                gx = start_x + lx
                if omatrix.get(lx, ly):
                    adj = calc_adjacency(omatrix, lx, ly)
                    matrix[gy * matrix_w + gx] = tex_lookup[adj]
    
    out_bytes = bytearray(matrix_w * matrix_h)
    byte_wp = 0
    for v in matrix:
        out_bytes[byte_wp] = v & 0xFF
        byte_wp += 1
    
    with ioutil.open_output(out_path, binary=True) as out_f:
        out_f.write(out_bytes)


def calc_room_automap_occupancy(room_path: str) -> Array2D:
    cell_w = WORLD_GRID_WIDTH / MAP_SCREEN_SUBDIV / 8.0
    cell_h = WORLD_GRID_HEIGHT / MAP_SCREEN_SUBDIV / 8.0

    with open(room_path + '.map', 'rb') as map_file:
        map_data = map_file.read()
    
    (room_width, room_height, _, _, col_data_offset, _, _) = \
        struct.unpack('<HHBxxxIIII', map_data[0:24])
    
    col_data = bytearray(int_ceil_div(room_width * room_height, 4) * 4)
    j = 0
    for i in range(0, int_ceil_div(room_width * room_height, 4)):
        byte = map_data[col_data_offset + i]
        col_data[j  ] = byte & 0x3
        col_data[j+1] = (byte >> 2) & 0x3
        col_data[j+2] = (byte >> 4) & 0x3
        col_data[j+3] = (byte >> 6) & 0x3
        j += 4
    
    room_swidth = int_ceil_div(room_width, ROOM_SCREEN_WIDTH)
    room_sheight = int_ceil_div(room_height, ROOM_SCREEN_HEIGHT)
    
    owidth = room_swidth * MAP_SCREEN_SUBDIV
    oheight = room_sheight * MAP_SCREEN_SUBDIV
    occupancy: list[bool] = [False] * (owidth * oheight)

    i = 0
    for r in range(0, room_sheight * MAP_SCREEN_SUBDIV):
        start_y = int(r * cell_h) + MAP_SCREEN_MARGIN_Y
        end_y = int(start_y + cell_h) - MAP_SCREEN_MARGIN_Y

        for c in range(0, room_swidth * MAP_SCREEN_SUBDIV):
            start_x = int(c * cell_w) + MAP_SCREEN_MARGIN_X
            end_x = int(start_x + cell_w) - MAP_SCREEN_MARGIN_X

            # assert(room.screens[i] ~= nil)

            scr_available = False
            for y in range(start_y, end_y + 1):
                assert (y >= 0 and y < room_height)
                for x in range(start_x, end_x + 1):
                    assert (x >= 0 and x < room_width)
                    if col_data[y * room_width + x] != 1:
                        scr_available = True
                        break
                else:
                    continue
                break

            occupancy[i] = scr_available
            i += 1
    
    return Array2D(owidth, oheight, occupancy)


def main():
    parser = argparse.ArgumentParser(prog='worldproc')
    parser.add_argument('world', help='input tiled .world file.')
    parser.add_argument('roomlist', help='input room_list.txt file.')
    parser.add_argument('--json', metavar='path', help='output json file. pass - to write to stdout.')
    parser.add_argument('--bin', metavar='path', help='output bin file. pass - to write to stdout.')
    parser.add_argument('--c', dest='c', metavar='path', help="output .h file for room list. sibling .c source will be generated as well.")
    parser.add_argument('--automap', help='output automap bin file. pass - to write to stdout.')

    args = parser.parse_args()

    with open(args.roomlist) as f:
        room_list: list[str] = []
        while True:
            line = f.readline()
            if not line: break
            if line[0] == '#': continue
            room_list.append(line.strip())

    world_path = path.abspath(args.world)

    out_json: typing.TextIO = None
    out_bin: typing.BinaryIO = None
    out_automap: str = None
    out_cp: str = None

    if args.json:
        out_json = ioutil.open_output(args.json)
    
    if args.bin:
        out_bin = ioutil.open_output(args.bin, binary=True)

    if args.c:
        out_cp = args.c

    if args.automap:
        out_automap = args.automap

    try:
        worldproc(world_path, room_list,
                  out_json=out_json,
                  out_bin=out_bin,
                  out_cp=out_cp,
                  out_automap=out_automap)
    finally:
        if out_json:
            out_json.close()
        if out_bin:
            out_bin.close()


if __name__ == '__main__':
    main()