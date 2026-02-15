#!/usr/bin/env python3
# script to compile tiled maps to the binary format used in the game.
import xml.etree.ElementTree as xml
import base64
import struct
import sys
import argparse
import os
import os.path as path
import json
import ioutil
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

def align(x: int, width: int) -> int:
    return (x + width - 1) // width * width

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

def parse(ifile_path: str, output_file: BinaryIO, tileset: Tileset,
          world_data: dict):
    with open(ifile_path, 'r') as ifile:
        file_contents = ifile.read()
    
    tmx_data = xml.fromstring(file_contents)
    layer = tmx_data.find('layer')
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

    room_name = path.splitext(path.basename(ifile_path))[0]
    if not room_name in world_data['rooms']:
        raise Exception(f"could not find '{room_name}' in world.json")

    room_data = world_data['rooms'][room_name]
    output_file.write(struct.pack('<HHBBxx', map_width, map_height,
                                  room_data['x'], room_data['y']))

    # write collision data
    byte_accum: list[int] = []
    col_data = bytearray()
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
            col_data += struct.pack('<B', out_byte)
            byte_accum.clear()
    
    if len(byte_accum) > 0:
        while len(byte_accum) < 4:
            byte_accum.append(0)

        out_byte = byte_accum[0] | (byte_accum[1] << 2) | (byte_accum[2] << 4) | (byte_accum[3] << 6)
        col_data += struct.pack('<B', out_byte)
        byte_accum.clear()
    
    # write graphics data
    gfx_data = bytearray()
    for i in range(0, map_width * map_height * 4, 4):
        tile_int = (data[i] | (data[i+1] << 8) |
                    (data[i+2] << 16) | (data[i+3] << 24))

        if tile_int == 0:
            out_int = 0
        else:
            flip_h = (tile_int & FLIPPED_HORIZONTALLY_FLAG) != 0
            flip_v = (tile_int & FLIPPED_VERTICALLY_FLAG) != 0
            tid = tile_int & 0x00FFFFFF

            if tid > 255:
                print(data_base64.text.strip())
                print(i // 4, tile_int)
                print((i // 4) % (map_width), i // (4 * map_width))
                tid = tile_int & 0x0FFFFFFF
            
            assert tid <= 255
            
            out_int = tid
            if flip_h:
                out_int = out_int | (1 << 8)
            if flip_v:
                out_int = out_int | (1 << 9)
        
        gfx_data += struct.pack('<H', out_int)
    
    obj_tmx = tmx_data.find('objectgroup')
    ent_data = None
    if obj_tmx is not None:
        ent_data = bytearray()

        # get list of entities
        entities: list[xml.Element[str]] = []
        for ent in obj_tmx.findall('object'):
            if ent.get('type') == 'entity':
                entities.append(ent)

        # write entity count
        ent_data += struct.pack('<H', len(entities))

        # write entity data
        for ent in entities:
            ent_name = ent.get('name')
            
            odata = bytearray()
            odata += struct.pack('<HHHH', int(ent.get('x')), int(ent.get('y')),
                                int(ent.get('width')), int(ent.get('height')))
            name_bytes = bytes(ent_name, 'ascii')
            odata += name_bytes
            odata.append(0)

            prop_tag = ent.find('properties')
            if prop_tag:
                ent_props = prop_tag.findall('property')
                odata += struct.pack('<B', len(ent_props))

                for prop in ent_props:
                    odata += bytes(prop.get('name'), 'ascii')
                    odata.append(0)

                    match prop.get('type', 'string'):
                        case 'string':
                            odata.append(0)
                            odata += bytes(prop.get('value'), 'ascii')
                            odata.append(0)

                        case 'int':
                            odata.append(1)
                            odata += struct.pack('<I', int(prop.get('value')))

                        case 'float':
                            odata.append(2)
                            fx_val = int(float(prop.get('value')) * 256)
                            odata += struct.pack('<I', fx_val)

                        case t:
                            raise Exception("unknown property type " + t)
            else:
                odata += struct.pack('<B', 0)

            ent_data += struct.pack('<H', len(odata))
            ent_data += odata
    
    section_offset = 20
    output_file.write(struct.pack('<I', section_offset)) # col offset
    section_offset += align(len(col_data), 4)
    output_file.write(struct.pack('<I', section_offset)) # gfx offset
    section_offset += align(len(gfx_data), 4)

    if ent_data:
        output_file.write(struct.pack('<I', section_offset)) # ent offset
    else:
        output_file.write(bytes([0]))

    bytes_written = 0

    output_file.write(col_data)
    bytes_written += len(col_data)
    while bytes_written % 4 != 0:
        output_file.write(bytes([0]))
        bytes_written += 1

    output_file.write(gfx_data)
    bytes_written += len(gfx_data)
    while bytes_written % 4 != 0:
        output_file.write(bytes([0]))
        bytes_written += 1

    if ent_data:
        output_file.write(ent_data)
        bytes_written += len(ent_data)
    # while bytes_written % 4 != 0:
    #     output_file.write(0)
    #     bytes_written += 1

def main():
    parser = argparse.ArgumentParser(prog='mapc')
    parser.add_argument('input', help="path to input tmx file.")
    parser.add_argument('world', help="path to worldproc json output.")
    parser.add_argument('output', help="output bin file. pass - to write to stdout.")

    args = parser.parse_args()

    out_file = ioutil.open_output(args.output, binary=True)

    with open(args.world, 'r') as wf:
        world_data = json.load(wf)

    s = False
    try:
        parse(args.input, out_file, parse_tileset(args.input), world_data)
        s = True
    finally:
        if not s:
            sys.stderr.write("error, deleting file "  + args.output)
            sys.stderr.write("\n")

            if not out_file.isatty():
                out_file.close()
                os.remove(args.output)
                out_file = None

    out_file.close()

if __name__ == '__main__': main()