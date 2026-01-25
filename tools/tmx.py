#!/usr/bin/env python
import xml.etree.ElementTree as xml
import base64
import struct
import sys
import argparse
from typing import BinaryIO, TextIO

FLIPPED_HORIZONTALLY_FLAG  = 0x80000000
FLIPPED_VERTICALLY_FLAG    = 0x40000000
FLIPPED_DIAGONALLY_FLAG    = 0x20000000
ROTATED_HEXAGONAL_120_FLAG = 0x10000000

def parse(input_file: TextIO, output_file: BinaryIO):
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

    output_file.write(struct.pack('<HH', map_width, map_height))

    for i in range(map_width * map_height):
        tile_int = (data[i*4] | (data[i*4+1] << 8) |
                    (data[i*4+2] << 16) | (data[i*4+3] << 24))        
        
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
    parser = argparse.ArgumentParser(prog='tmx')
    parser.add_argument('input', help='Input tmx file. Pass - to read from stdin.')
    parser.add_argument('output', help='Output bin file. Pass - to read from stdout.')

    args = parser.parse_args()
    print(args.input, args.output)

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

    parse(in_file, out_file)

    if not use_stdin:
        in_file.close()

    if not use_stdout:
        out_file.close()

if __name__ == '__main__': main()