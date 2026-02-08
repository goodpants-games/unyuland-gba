#!/usr/bin/env python3
import sys
import os.path as path
import argparse
import json
import typing
import struct

WORLD_GRID_WIDTH = 15 * 8
WORLD_GRID_HEIGHT = 11 * 8

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


def abort_errors():
    if len(g_errors) == 1:
        raise g_errors[0]
    else:
        err_text = ""
        for err in g_errors:
            err_text += "\n\t" + str(err)
        raise RuntimeError("multiple errors occurred: " + err_text)


def worldproc(world_path: str, room_list: list[str], out_json: typing.TextIO,
              out_bin: typing.BinaryIO):
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

    out_bin.write(struct.pack('<HH', matrix_w, matrix_h))
    out_bin.write(matrix)

def main():
    parser = argparse.ArgumentParser(prog='worldproc')
    parser.add_argument('world', help='input tiled .world file.')
    parser.add_argument('roomlist', help='input room_list.txt file.')
    parser.add_argument('out_json', help='output json file. pass - to write to stdout.')
    parser.add_argument('out_bin', help='output bin file. pass - to write to stdout.')

    args = parser.parse_args()

    with open(args.roomlist) as f:
        room_list: list[str] = []
        while True:
            line = f.readline()
            if not line: break
            if line[0] == '#': continue
            room_list.append(line.strip())

    world_path = path.abspath(args.world)
    json_use_stdout = args.out_json == '-'
    bin_use_stdout = args.out_bin == '-'

    if json_use_stdout:
        out_json = sys.stdout
    else:
        out_json = open(args.out_json, 'w')

    if bin_use_stdout:
        out_bin = sys.stdout.buffer
    else:
        out_bin = open(args.out_bin, 'wb')

    try:
        worldproc(world_path, room_list, out_json, out_bin)
    finally:
        if not json_use_stdout:
            out_json.close()

        if not bin_use_stdout:
            out_bin.close()


if __name__ == '__main__':
    main()