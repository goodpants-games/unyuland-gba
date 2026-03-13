#!/usr/bin/env python3
import colorsys
import sys
import argparse

def rgb_from_hex(hex: int) -> tuple[float, float, float]:
    b = hex & 0xFF
    g = (hex >> 8) & 0xFF
    r = (hex >> 16) & 0xFF
    return (r / 255, g / 255, b / 255)


def clamp(v: float, min: float, max: float) -> float:
    if v < min: return min
    if v > max: return max
    return v


def clamp01(v: float) -> float:
    return max(min(v, 1.0), 0.0)


def rgb_to_r5g5b5(rgb: tuple[float, float, float]) -> int:
    r, g, b = rgb
    r = clamp01(r)
    g = clamp01(g)
    b = clamp01(b)

    return (  (int(round(r * 0x1F)) & 0x1F)
            | ((int(round(g * 0x1F)) & 0x1F) << 5)
            | ((int(round(b * 0x1F)) & 0x1F) << 10))


def proc_color(rgb: tuple[float,float,float], smul: float, vmul: float):
    r, g, b = rgb
    h, s, v = colorsys.rgb_to_hsv(r, g, b)
    s = clamp01(s * smul)
    v = clamp01(v * vmul)

    r, g, b = colorsys.hsv_to_rgb(h, s, v)

    return (r, g, b)
    # scale_fac = 1.4

    # if r > g and r > b:
    #     r = r * scale_fac
    #     g = g / scale_fac
    #     b = b / scale_fac
    # elif g > r and g > b:
    #     r = r / scale_fac
    #     g = g * scale_fac
    #     b = b / scale_fac
    # elif b > r and b > g:
    #     r = r / scale_fac
    #     g = g / scale_fac
    #     b = b * scale_fac
    # else:
    #     r = r * scale_fac
    #     g = g * scale_fac
    #     b = b *scale_fac
    
    # r = max(min(r, 1.0), 0.0)
    # g = max(min(g, 1.0), 0.0)
    # b = max(min(b, 1.0), 0.0)

    # return (r, g, b)


PICO8_COLORS_NORMAL = [
    rgb_from_hex(0x000000), # black
    rgb_from_hex(0x1d2b53), # dark_blue
    rgb_from_hex(0x7e2553), # dark_purple
    rgb_from_hex(0x008751), # dark_green
    rgb_from_hex(0xab5236), # brown
    rgb_from_hex(0x5f574f), # dark_gray
    rgb_from_hex(0xc2c3c7), # light_gray
    rgb_from_hex(0xfff1e8), # white
    rgb_from_hex(0xff004d), # red
    rgb_from_hex(0xffa300), # orange
    rgb_from_hex(0xffec27), # yellow
    rgb_from_hex(0x00e436), # green
    rgb_from_hex(0x29adff), # blue
    rgb_from_hex(0x83769c), # indigo
    rgb_from_hex(0xff77a8), # pink
    rgb_from_hex(0xffccaa), # peach
]

PICO8_COLORS_LCD = [
    rgb_from_hex(0x000000), # black
    # rgb_from_hex(0x1d2b53), # dark_blue
    rgb_from_hex(0x2d0063),
    # rgb_from_hex(0x7e2553), # dark_purple
    rgb_from_hex(0x7e2564),
    # rgb_from_hex(0x008751), # dark_green
    rgb_from_hex(0x008767),
    rgb_from_hex(0xab5236), # brown
    rgb_from_hex(0x5f574f), # dark_gray
    rgb_from_hex(0xc2c3c7), # light_gray
    rgb_from_hex(0xfff1e8), # white
    rgb_from_hex(0xff004d), # red
    rgb_from_hex(0xffa300), # orange
    rgb_from_hex(0xffec27), # yellow
    rgb_from_hex(0x00e436), # green
    rgb_from_hex(0x29adff), # blue
    rgb_from_hex(0x83769c), # indigo
    rgb_from_hex(0xff77a8), # pink
    rgb_from_hex(0xffccaa), # peach
]


def proc(colors: list[int], smul: float, vmul: float) -> None:
    first = True

    for color in colors:
        if not first:
            sys.stdout.write(",\n")
        first = False
        
        color = proc_color(color, smul, vmul)
        r5g5b5 = rgb_to_r5g5b5(color)
        str = "0x{:04x}".format(r5g5b5)
        sys.stdout.write(str)
    
    sys.stdout.write("\n")

def main() -> None:
    parser = argparse.ArgumentParser('color')
    parser.add_argument('-m', '--mode', help="mode: 'normal' or 'lcd'")

    args = parser.parse_args()

    if args.mode == 'normal':
        colors = PICO8_COLORS_NORMAL
        smul = 1.0
        vmul = 1.0
    elif args.mode == 'lcd':
        colors = PICO8_COLORS_LCD
        smul = 1.3
        vmul = 1.2
    else:
        print("invalid mode", file=sys.stderr)
        exit(1)
    
    proc(colors, smul, vmul)

if __name__ == '__main__':
    main()