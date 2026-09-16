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
from typing import BinaryIO, TextIO, Self

FLIPPED_HORIZONTALLY_FLAG  = 0x80000000
FLIPPED_VERTICALLY_FLAG    = 0x40000000
FLIPPED_DIAGONALLY_FLAG    = 0x20000000
ROTATED_HEXAGONAL_120_FLAG = 0x10000000

class Tileset:
    def __init__(self: Self, name: str, firstgid: int):
        self.name = name
        self.firstgid: int = firstgid
        self.data: dict[int, str] = {}


    def set_data(self: Self, id: int, type: str):
        self.data[id] = type


    def get_data(self: Self, id: int) -> str:
        if id in self.data:
            return self.data[id]
        else:
            return None


class TilesetCollection:
    def __init__(self: Self):
        self.sets: list[Tileset] = []

    # returns: (tileset, local_id). returns (None, -1) on error.
    def to_local(self: Self, id: int) -> tuple[Tileset, int]:
        if id == 0:
            return (None, -1)
        
        for i in range(len(self.sets)-1, -1, -1):
            set = self.sets[i]
            if id >= set.firstgid:
                return (set, id - set.firstgid)
        return (None, -1)


def align(x: int, width: int) -> int:
    return (x + width - 1) // width * width


def parse_tileset(root: xml.Element, firstgid: int) -> Tileset:
    output = Tileset(root.get('name'), firstgid)

    for tile in root.findall('tile'):
        id = int(tile.get('id'))
        output.set_data(id, tile.get('type'))

    return output


def parse_tilesets(tmx_path: str, tmx_data: xml.Element) -> TilesetCollection:
    output = TilesetCollection()

    for xtileset in tmx_data.findall('tileset'):
        firstgid = int(xtileset.get('firstgid'))

        tileset_src = xtileset.get('source')
        if tileset_src is None:
            tileset = parse_tileset(xtileset, firstgid)
        else:
            tsx_path = path.join(path.dirname(tmx_path), tileset_src)
            with open(tsx_path, 'r') as file:
                tileset = parse_tileset(xml.fromstring(file.read()), firstgid)

        output.sets.append(tileset)
    
    return output

def parse(ifile_path: str, output_file: BinaryIO):
    with open(ifile_path, 'r') as ifile:
        file_contents = ifile.read()
    
    tmx_data = xml.fromstring(file_contents)
    tilesets = parse_tilesets(ifile_path, tmx_data)

    # determine if the room is "outdoors"
    # i.e., it has a property named "outdoors" that is a true boolean value
    is_outdoors = False
    room_props = tmx_data.find('properties')
    if room_props is not None:
        for prop in room_props.findall('property'):
            if prop.get('name') == 'outdoors' and prop.get('type') == 'bool':
                is_outdoors = prop.get('value') == 'true'

    # the level is valid only if:
    #   1. there are two layers, where one's class is "flags", and the other
    #      has no class.
    #   2. there is only one layer, with no class.
    # find normal tile layer and flags layer.
    flags_layer = None
    tile_layer = None
    for l in tmx_data.findall('layer'):
        if l.get('class') == 'flags':
            if flags_layer != None:
                raise Exception("map has multiple flag layers!")
            flags_layer = l
        else:
            if tile_layer != None:
                raise Exception("map has multiple regular tile layers!")
            tile_layer = l
    
    if tile_layer is None:
        raise Exception("map has no tile layer")

    map_width = int(tile_layer.get('width'))
    map_height = int(tile_layer.get('height'))

    # read data for regular tile layer into tdata
    tdata_base64 = tile_layer.find('data')
    if tdata_base64 is None:
        raise Exception("tile layer has no data")
    if tdata_base64.get('encoding') != 'base64':
        raise Exception("only base64 encoding is supported")
    tdata = base64.b64decode(tdata_base64.text.strip())

    # read data for flags tile layer into fdata. if the layer does not exist,
    # create a dummy zero-filled array.
    if flags_layer is not None:
        fdata_base64 = flags_layer.find('data')
        if fdata_base64 is None:
            raise Exception("flags layer has no data")
        if fdata_base64.get('encoding') != 'base64':
            raise Exception("only base64 encoding is supported")
        fdata = base64.b64decode(fdata_base64.text.strip())
    else:
        fdata = bytes(map_width * map_height * 4)

    room_name = path.splitext(path.basename(ifile_path))[0]
    output_file.write(struct.pack('<HHBxxx', map_width, map_height,
                                  1 if is_outdoors else 0))

    # get collision matrix
    col_data: list[int] = []
    for i in range(0, map_width * map_height * 4, 4):
        # get global ID at regular tilemap
        tile_int = (tdata[i] | (tdata[i+1] << 8) |
                   (tdata[i+2] << 16) | (tdata[i+3] << 24))
        gid = tile_int & 0x0FFFFFFF
        # get global ID at flags tilemap
        flag_int = (fdata[i] | (fdata[i+1] << 8) |
                   (fdata[i+2] << 16) | (fdata[i+3] << 24))
        cid = 3 if is_outdoors else 0

        if gid != 0:
            (tileset, tid) = tilesets.to_local(gid)
            assert tileset.name == "tileset"
            type_str = tileset.get_data(tid)
            match type_str:
                case 'water':
                    cid = 2
                case 'heat':
                    cid = 3
                case 'decor':
                    cid = 0
                case _: # solid
                    (flag_tileset, fid) = tilesets.to_local(flag_int)
                    if flag_tileset:
                        assert flag_tileset.name == "flags_tileset"
                    
                    if fid == 1:
                        cid = 4 # semi-solid
                    else:
                        cid = 1 # full solid

        assert cid <= 4
        col_data.append(cid)
    
    # extend water downwards into air
    for y in range(0, map_height - 1):
        for x in range(0, map_width):
            i1 = y * map_width + x
            i2 = (y + 1) * map_width + x
            if (col_data[i1] == 2 and col_data[i2] == 0):
                col_data[i2] = 2

    # write collision data into a packed byte array. 4 bits per cell.
    byte_accum: list[int] = []
    col_bytes = bytearray()
    for i in range(0, map_width * map_height):
        cid = col_data[i]
        byte_accum.append(cid & 0xF)

        if len(byte_accum) == 2:
            out_byte = byte_accum[0] | (byte_accum[1] << 4)
            col_bytes += struct.pack('<B', out_byte)
            byte_accum.clear()
    
    if len(byte_accum) > 0:
        while len(byte_accum) < 2:
            byte_accum.append(0)

        out_byte = byte_accum[0] | (byte_accum[1] << 4)
        col_bytes += struct.pack('<B', out_byte)
        byte_accum.clear()
    
    # write graphics data
    gfx_data = bytearray()
    for i in range(0, map_width * map_height * 4, 4):
        tile_int = (tdata[i] | (tdata[i+1] << 8) |
                   (tdata[i+2] << 16) | (tdata[i+3] << 24))

        if tile_int == 0:
            out_int = 0
        else:
            (tileset, tid) = tilesets.to_local(tile_int & 0x00FFFFFF)
            assert tileset.name == 'tileset'

            flip_h = (tile_int & FLIPPED_HORIZONTALLY_FLAG) != 0
            flip_v = (tile_int & FLIPPED_VERTICALLY_FLAG) != 0

            #if tid > 255:
            #    print(tdata_base64.text.strip())
            #    print(i // 4, tile_int)
            #    print((i // 4) % (map_width), i // (4 * map_width))
        
            assert tid <= 255
            
            out_int = tid + 1
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
            odata += struct.pack('<HHHH', int(float(ent.get('x'))),
                                 int(float(ent.get('y'))),
                                 int(ent.get('width')), int(ent.get('height')))
            name_bytes = bytes(ent_name, 'ascii')
            odata += name_bytes
            odata.append(0)

            prop_tag = ent.find('properties')
            if prop_tag is not None:
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
    section_offset += align(len(col_bytes), 4)
    output_file.write(struct.pack('<I', section_offset)) # gfx offset
    section_offset += align(len(gfx_data), 4)

    if ent_data:
        output_file.write(struct.pack('<I', section_offset)) # ent offset
    else:
        output_file.write(bytes([0,0,0,0]))

    bytes_written = 0

    output_file.write(col_bytes)
    bytes_written += len(col_bytes)
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
    parser.add_argument('output', help="output bin file. pass - to write to stdout.")

    args = parser.parse_args()

    out_file = ioutil.open_output(args.output, binary=True)

    s = False
    try:
        parse(args.input, out_file)
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