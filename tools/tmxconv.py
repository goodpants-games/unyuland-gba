#!/usr/bin/env python3
# script to convert original unyuland tiled maps to unyuland advance tiled maps.
# the only change between the two is that it uses a different tileset.
import xml.etree.ElementTree as ET
import base64
import sys
import argparse
import os.path as path
from typing import BinaryIO, TextIO

HFLIP_FLAG = 0x80000000
VFLIP_FLAG = 0x40000000
DFLIP_FLAG = 0x20000000

TILE_ID_MASK    = 0x0FFFFFFF
TILE_FLAGS_MASK = 0xF0000000

# 90-degree clockwise rotations
ROT1_FLAG = HFLIP_FLAG | DFLIP_FLAG
ROT2_FLAG = HFLIP_FLAG | VFLIP_FLAG
ROT3_FLAG = VFLIP_FLAG | DFLIP_FLAG

id_map: dict[int, int] = dict([
    # solid block
    (0, 0),

    # basic edges
    (1,              1),
    (1 | ROT1_FLAG,  2),
    (1 | ROT2_FLAG,  1 | HFLIP_FLAG | VFLIP_FLAG),
    (1 | ROT3_FLAG,  2 | HFLIP_FLAG | VFLIP_FLAG),

    # basic outer corners
    (2,             3),
    (2 | ROT1_FLAG, 3 | VFLIP_FLAG),
    (2 | ROT2_FLAG, 3 | HFLIP_FLAG | VFLIP_FLAG),
    (2 | ROT3_FLAG, 3 | HFLIP_FLAG),

    # basic inner corners
    (3,             4),
    (3 | ROT1_FLAG, 4 | VFLIP_FLAG),
    (3 | ROT2_FLAG, 4 | HFLIP_FLAG | VFLIP_FLAG),
    (3 | ROT3_FLAG, 4 | HFLIP_FLAG),

    # wall in water fade
    (4,              5),
    (4 | HFLIP_FLAG, 5 | HFLIP_FLAG),

    # cobblestone
    (37, 6),

    # water
    (16, 16),
    (17, 17),
    (32, -1),

    # background gaps
    (35, 18),
    (18, 19),
    (19, 20),
    (20, 21),
    (34, 22),
    (36, 23),
    (50, 24),
    (51, 25),
    (52, 26),

    # bg gap corners
    (53, 27),
    (54, 28),
    (69, 29),
    (70, 30),

    # background wall edges
    (66, 31),
    (67, 32),
    (68, 33),
    (82, 34),
    (83, -1),
    (84, 35),
    (98, 36),
    (99, 37),
    (100, 38),

    # decor
    (21, 39),
    (22, 40),
    (23, 41),
    (24, 42),
    (25, 43),
    (26, 44),
    (27, 45),
    (28, 46),
    (29, 47),
    (30, 48),
    
    (38, 49),
    (39, 50),
    (40, 51),
    (41, 52),
    (42, 53),
    (43, 54),
])


def write_tmx(ofile: TextIO, map_width: int, map_height: int, map_data: bytes,
              object_group: ET.Element|None):
    root = ET.Element('map')
    root.set('version', '1.10')
    root.set('orientation', 'orthogonal')
    root.set('renderorder', 'right-down')
    root.set('width', str(map_width))
    root.set('height', str(map_height))
    root.set('tilewidth', '8')
    root.set('tileheight', '8')
    root.set('infinite', '0')

    tileset = ET.Element('tileset')
    root.append(tileset)
    tileset.set('firstgid', '1')
    tileset.set('source', 'tileset.tsx')

    layer = ET.Element('layer')
    root.append(layer)
    layer.set('id', '1')
    layer.set('name', "Tiles")
    layer.set('width', str(map_width))
    layer.set('height', str(map_height))

    layer_data = ET.Element('data')
    layer.append(layer_data)
    layer_data.set('encoding', 'base64')
    layer_data.text = '\n   ' + base64.b64encode(map_data).decode('utf-8') + '\n  '

    if object_group != None:
        objects = ET.Element('objectgroup')
        root.append(objects)
        objects.set('id', '2')
        objects.set('name', "Objects")

        for o in object_group:
            objects.append(o)
    
    tree = ET.ElementTree(root)
    ET.indent(tree, " ", level=0)
    tree.write(ofile, 'UTF-8', True)


def convert(ifile_path: str, ofile: TextIO):
    with open(ifile_path, 'r') as ifile:
        ifile_txt = ifile.read()
    
    in_tmx = ET.fromstring(ifile_txt)
    tilesets = in_tmx.findall('tileset')
    if len(tilesets) != 1:
        raise RuntimeError("only one tileset is allowed in the tmx file")
    
    tile_layer = in_tmx.find('layer')
    if tile_layer == None:
        raise RuntimeError("could not find a tile layer")
    
    map_width = int(tile_layer.get('width'))
    map_height = int(tile_layer.get('height'))
    
    xml_map_data = tile_layer.find('data')
    if xml_map_data == None or xml_map_data.get('encoding') != 'base64':
        raise RuntimeError("tile layer did not have base64 data")
    
    map_data = base64.b64decode(xml_map_data.text.strip())
    omap_data: list[int] = []

    for i in range(0, map_width * map_height * 4, 4):
        tile_int = (map_data[i] | (map_data[i+1] << 8) |
                    (map_data[i+2] << 16) | (map_data[i+3] << 24))
        if tile_int == 0:
            omap_data.append(0)
            continue
        
        tid = tile_int & TILE_ID_MASK
        tflags = tile_int & TILE_FLAGS_MASK

        new_tile_int = id_map[(tid - 1) | tflags]
        if new_tile_int == -1:
            omap_data.append(0)
        else:
            new_tid = new_tile_int & TILE_ID_MASK
            new_tflags = new_tile_int & TILE_FLAGS_MASK
            omap_data.append((new_tid + 1) | new_tflags)

    assert len(omap_data) == map_width * map_height

    omap_bytes: bytearray = bytearray(len(omap_data) * 4)
    j = 0
    for i in range(0, len(omap_data)):
        v = omap_data[i]
        omap_bytes[j  ] = (v         & 0xFF)
        omap_bytes[j+1] = ((v >> 8)  & 0xFF)
        omap_bytes[j+2] = ((v >> 16) & 0xFF)
        omap_bytes[j+3] = ((v >> 24) & 0xFF)
        j += 4
    
    write_tmx(ofile, map_width, map_height, bytes(omap_bytes),
              in_tmx.find('objectgroup'))


def main():
    parser = argparse.ArgumentParser(prog='tmxconv')
    parser.add_argument('input', help='input tmx file.')
    parser.add_argument('output', help='output tmx file. pass - to write to stdout.')

    args = parser.parse_args()

    use_stdout = args.output == '-'

    ifile_path = path.abspath(args.input)
    if use_stdout:
        out_file = sys.stdout.buffer
    else:
        out_file = open(args.output, 'wb')

    try:
        convert(ifile_path, out_file)
    finally:
        if not use_stdout:
            out_file.close()


if __name__ == '__main__':
    main()