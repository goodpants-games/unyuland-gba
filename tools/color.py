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
    parser.add_argument('-c', '--conv', help="convert color to r5g5b5 format")

    args = parser.parse_args()

    if args.conv is not None:
        hex_color = int(args.conv, 16)
        print("0x{:04x}".format(rgb_to_r5g5b5(rgb_from_hex(hex_color))))
        return

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

# shader.glsl
"""
uniform mat4 color_matrix;
uniform mat4 inv_color_matrix;

const float darken_screen = 0.5;
const float target_gamma = 2.2;
const float display_gamma = 2.5;
const float lum = 0.99;
const float contrast = 1.0;
const vec3 r = vec3(0.84, 0.09, 0.15);
const vec3 g = vec3(0.18, 0.67, 0.10);
const vec3 b = vec3(0.0, 0.26, 0.73);

vec4 gbaColor(vec4 pCol) {
	vec4 screen = pow(pCol, vec4(target_gamma + darken_screen)).rgba;
	vec4 avglum = vec4(0.5);
	screen = mix(screen, avglum, (1.0 - contrast));
	
	screen = clamp(screen * lum, 0.0, 1.0);
	screen = color_matrix * screen;
	return pow(screen, vec4(1.0 / display_gamma + (darken_screen * 0.125)));
}

vec4 revGbaColor(vec4 pCol) {
	pCol = pow(pCol, vec4(1.0 / (1.0 / display_gamma + (darken_screen * 0.125))));
	pCol = inv_color_matrix * pCol;
	pCol /= lum;
	
	// v = col * (1 - a) + C * a
	// v - C * a = col * (1 - a)
	// (v - C * a) / (1 - a) = col
	vec4 avglum = vec4(0.5);
	pCol = (pCol - avglum * (1.0 - contrast)) / contrast;
	pCol = pow(pCol, vec4(1.0 / (target_gamma + darken_screen)));
	
	return clamp(pCol, 0.0, 1.0);
}

vec4 effect(vec4 color, Image tex, vec2 texture_coords, vec2 screen_coords) {
	vec4 col = Texel(tex, texture_coords) * color;
	col = floor(col * 31.0 + vec4(0.5)) / 31.0;
	col = revGbaColor(col);
	return vec4(col.rgb, 1.0);
}
"""

# main.lua
"""
--[[
varying vec2 texCoord;
uniform sampler2D tex;
uniform vec2 texSize;

uniform float darken_screen;
const float target_gamma = 2.2;
const float display_gamma = 2.5;
const float sat = 1.0;
const float lum = 0.99;
const float contrast = 1.0;
const vec3 bl = vec3(0.0, 0.0, 0.0);
const vec3 r = vec3(0.84, 0.09, 0.15);
const vec3 g = vec3(0.18, 0.67, 0.10);
const vec3 b = vec3(0.0, 0.26, 0.73);

vec4 gbaColor(vec4 pCol) {
	vec4 screen = pow(pCol, vec4(target_gamma + darken_screen)).rgba;
	vec4 avglum = vec4(0.5);
	screen = mix(screen, avglum, (1.0 - contrast));
 
	mat4 color = mat4(	r.r,	r.g,	r.b,	0.0,
				g.r,	g.g,	g.b,	0.0,
				b.r,	b.g,	b.b,	0.0,
				bl.r,	bl.g,	bl.b,	1.0);
			  
	mat4 adjust = mat4(	(1.0 - sat) * 0.3086 + sat,	(1.0 - sat) * 0.3086,		(1.0 - sat) * 0.3086,		1.0,
				(1.0 - sat) * 0.6094,		(1.0 - sat) * 0.6094 + sat,	(1.0 - sat) * 0.6094,		1.0,
				(1.0 - sat) * 0.0820,		(1.0 - sat) * 0.0820,		(1.0 - sat) * 0.0820 + sat,	1.0,
				0.0,				0.0,				0.0,				1.0);
	color *= adjust;
	screen = clamp(screen * lum, 0.0, 1.0);
	screen = color * screen;
	return pow(screen, vec4(1.0 / display_gamma + (darken_screen * 0.125)));
}
--]]

local mat4 = require("mat4")
local bit = require("bit")

Lg = love.graphics

local mesh = Lg.newMesh({
	{0,  0,   0,0, 0,0,1,1},
	{255,0,   0,0, 1,0,1,1},
	{255,255, 0,0, 1,1,1,1},
	{0,255,   0,0, 0,1,1,1}
})

local mat_color = mat4.construct(
	0.84, 0.09, 0.15, 0.0,
	0.18, 0.67, 0.10, 0.0,
	0.0,  0.26, 0.73, 0.0,
	0.0,  0.0,  0.0,  1.0)
mat_color = mat_color:transpose()

local sat = 1.0
local mat_adjust = mat4.construct(
	(1.0 - sat) * 0.3086 + sat, (1.0 - sat) * 0.3086,       (1.0 - sat) * 0.3086,       1.0,
	(1.0 - sat) * 0.6094,       (1.0 - sat) * 0.6094 + sat, (1.0 - sat) * 0.6094,       1.0,
	(1.0 - sat) * 0.0820,       (1.0 - sat) * 0.0820,       (1.0 - sat) * 0.0820 + sat, 1.0,
	0.0,                        0.0,                        0.0,                        1.0)
mat_adjust = mat_color:transpose()

local final_mat = mat_color * mat_adjust
local inv_final_mat = final_mat:inverse()

local shader = Lg.newShader("shader.glsl")

local function unpack_rgb(rgb)
	local r = bit.rshift(bit.band(rgb, 0x00ff0000), 16) / 255
	local g = bit.rshift(bit.band(rgb, 0x0000ff00), 8)  / 255
	local b = bit.rshift(bit.band(rgb, 0x000000ff), 0)  / 255
	return r, g, b
end

P8_PAL = {
    { unpack_rgb(0x000000) }, -- black
    { unpack_rgb(0x1d2b53) }, -- dark_blue
    { unpack_rgb(0x7e2553) }, -- dark_purple
    { unpack_rgb(0x008751) }, -- dark_green
    { unpack_rgb(0xab5236) }, -- brown
    { unpack_rgb(0x5f574f) }, -- dark_gray
    { unpack_rgb(0xc2c3c7) }, -- light_gray
    { unpack_rgb(0xfff1e8) }, -- white
    { unpack_rgb(0xff004d) }, -- red
    { unpack_rgb(0xffa300) }, -- orange
    { unpack_rgb(0xffec27) }, -- yellow
    { unpack_rgb(0x00e436) }, -- green
    { unpack_rgb(0x29adff) }, -- blue
    { unpack_rgb(0x83769c) }, -- indigo
    { unpack_rgb(0xff77a8) }, -- pink
    { unpack_rgb(0xffccaa) }, -- peach
}

if shader:hasUniform("color_matrix") then
	shader:send("color_matrix", final_mat)
end

if shader:hasUniform("inv_color_matrix") then
	shader:send("inv_color_matrix", final_mat:inverse())
end

local function draw_p8_squares(x)
	for i=1, #P8_PAL do
		Lg.setColor(unpack(P8_PAL[i]))
		Lg.rectangle("fill", x, 255 + (i-1) * 16, 80, 16)
	end
end

function love.draw()
	local b = love.mouse.getX() / love.graphics.getWidth()
	Lg.setColor(1, 1, b)
	
	Lg.setShader(shader)
	Lg.draw(mesh, 0, 0)
	draw_p8_squares(0)
	
	Lg.setShader()
	Lg.draw(mesh, 255, 0)
	draw_p8_squares(255)
--[[
	local b = love.mouse.getX() / love.graphics.getWidth()
	for y=0, 255 do
		for x=0, 255 do
			local r, g, b = x / 255, y / 255, b
			Lg.setColor(r, g, b)
			Lg.points(x + 255, y)
			r, g, b = map_color(r, g, b)
			Lg.setColor(r, g, b)
			Lg.points(x, y)
		end
	end
--]]
end
"""