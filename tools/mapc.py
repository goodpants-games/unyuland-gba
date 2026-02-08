#!/usr/bin/env python3
# script to compile tiled maps to the binary format used in the game.
import xml.etree.ElementTree as xml
import base64
import struct
import sys
import argparse
import os.path as path
from typing import BinaryIO, TextIO

FLIPPED_HORIZONTALLY_FLAG  = 0x80000000
FLIPPED_VERTICALLY_FLAG    = 0x40000000
FLIPPED_DIAGONALLY_FLAG    = 0x20000000
ROTATED_HEXAGONAL_120_FLAG = 0x10000000

class Tileset:
    def __init__(self):
        self.data: dict[int, str] = {}

    def register(self, id: int, type: str):
        self.data[id] = type

    def get(self, id: int) -> str:
        if id in self.data:
            return self.data[id]
        else:
            return None

def parse_tileset(tmx_path: str):
    tsx_path = path.join(path.dirname(tmx_path), 'tileset.tsx')
    with open(tsx_path, 'r') as file:
        file_contents = file.read()

    output = Tileset()
    data = xml.fromstring(file_contents)
    for tile in data.findall('tile'):
        id = int(tile.get('id'))
        output.register(id, tile.get('type'))

    return output

def parse(input_file: TextIO, output_file: BinaryIO, tileset: Tileset):
    file_contents = input_file.read()
    
    data = xml.fromstring(file_contents)
    layer = data.find('layer')
    if layer is None:
        raise Exception("map has no tile layer")

    map_width = int(layer.get('width'))
    map_height = int(layer.get('height'))

    data_base64 = layer.find('data')
    if data_base64 is None:
        raise Exception("tile layer has no data")
    
    if data_base64.get('encoding') != 'base64':
        raise Exception("only base64 encoding is supported")
    
    data = base64.b64decode(data_base64.text.strip())

    output_file.write(struct.pack('<HHHxx', map_width, map_height, 0))

    # write collision data
    bytes_written = 0
    byte_accum: list[int] = []
    for i in range(0, map_width * map_height * 4, 4):
        tile_int = (data[i] | (data[i+1] << 8) |
                    (data[i+2] << 16) | (data[i+3] << 24))
        tid = tile_int & 0x0FFFFFFF
        cid = 0

        if tid != 0:
            type_str = tileset.get(tid - 1)
            match type_str:
                case 'water':
                    cid = 2
                case 'heat':
                    cid = 3
                case 'decor':
                    cid = 0
                case _:
                    cid = 1

        assert cid <= 4
        byte_accum.append(cid)

        if len(byte_accum) == 4:
            out_byte = byte_accum[0] | (byte_accum[1] << 2) | (byte_accum[2] << 4) | (byte_accum[3] << 6)
            output_file.write(struct.pack('<B', out_byte))
            byte_accum.clear()
            bytes_written += 1
    
    if len(byte_accum) > 0:
        while len(byte_accum) < 4:
            byte_accum.append(0)

        out_byte = byte_accum[0] | (byte_accum[1] << 2) | (byte_accum[2] << 4) | (byte_accum[3] << 6)
        output_file.write(struct.pack('<B', out_byte))
        byte_accum.clear()
        bytes_written += 1

    while bytes_written % 4 != 0:
        output_file.write(struct.pack('x'))
        bytes_written += 1
    
    # write graphics data
    for i in range(0, map_width * map_height * 4, 4):
        tile_int = (data[i] | (data[i+1] << 8) |
                    (data[i+2] << 16) | (data[i+3] << 24))        
        
        flip_h = (tile_int & FLIPPED_HORIZONTALLY_FLAG) != 0
        flip_v = (tile_int & FLIPPED_VERTICALLY_FLAG) != 0
        tid = tile_int & 0x0FFFFFFF
        assert tid <= 255

        if tid > 0:
            tid = tid - 1
        
        out_int = tid
        if flip_h:
            out_int = out_int | (1 << 8)
        if flip_v:
            out_int = out_int | (1 << 9)
        
        output_file.write(struct.pack('<H', out_int))

def main():
    parser = argparse.ArgumentParser(prog='mapc')
    parser.add_argument('input', help='Input tmx file. Pass - to read from stdin.')
    parser.add_argument('output', help='Output bin file. Pass - to read from stdout.')

    args = parser.parse_args()

    use_stdin = args.input == '-'
    use_stdout = args.output == '-'

    if use_stdin:
        in_file = sys.stdin.buffer
    else:
        in_file = open(args.input, 'r')

    if use_stdout:
        out_file = sys.stdout.buffer
    else:
        out_file = open(args.output, 'wb')

    parse(in_file, out_file, parse_tileset(args.input))

    if not use_stdin:
        in_file.close()

    if not use_stdout:
        out_file.close()

if __name__ == '__main__': main()