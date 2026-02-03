--[[
// pointers are relative to gfx_data_root
// native alignment is 4 bytes

struct gfx_data_root {
    gfx_frame *frame_pool;
    gfx_frame *obj_pool;
    
    gfx_data  gfx[?];
    gfx_frame frame_pool_data[?];
    gfx_obj   obj_pool_data[?];
};

struct gfx_data {
    u8 frame_count;
    u8 loop; // bool
    u16 frame_pool_idx;
};

struct gfx_frame {
    u16 obj_pool_index;
    u8 frame_len;
    u8 obj_count;
};

struct gfx_obj {
    u16 a0; // contains only config for the sprite shape
    u16 a1; // contains only config for the sprite size
    u16 a2; // contains only config for the character index
    s8 ox;
    s8 oy;
};
--]]

local OUTPUT_WIDTH = 128
local OUTPUT_HEIGHT = 64

local ATTR0_SQUARE  = 0
local ATTR0_WIDE    = 0x4000
local ATTR0_TALL    = 0x8000
local ATTR1_SIZE_8  = 0
local ATTR1_SIZE_16 = 0x4000
local ATTR1_SIZE_32 = 0x8000
local ATTR1_SIZE_64 = 0xC000

local input_file_list = {}
local pico8_pal = Palette { fromResource = "PICO-8" }

local output_sprite = Sprite(OUTPUT_WIDTH, OUTPUT_HEIGHT, ColorMode.INDEXED)
output_sprite:setPalette(pico8_pal)

local output = Image(OUTPUT_WIDTH, OUTPUT_HEIGHT, ColorMode.INDEXED)
local out_idx = 0

local output_data = {}

local tinsert = table.insert

local function tappend(t, ...)
    local tinsert = table.insert
    for i=1, select("#", ...) do
        local v = select(i, ...)
        tinsert(t, v)
    end
end

local function img_blit(dst, src, p_dx, p_dy, p_sx, p_sy, p_sw, p_sh)
    local draw_pixel = dst.drawPixel
    local get_pixel = src.getPixel

    assert(p_dx >= 0)
    assert(p_dy >= 0)
    assert(p_sx >= 0)
    assert(p_sy >= 0)
    assert(p_sw >= 0)
    assert(p_sh >= 0)

    local sy = p_sy
    for dy=p_dy, p_dy + p_sh - 1 do
        local sx = p_sx
        for dx=p_dx, p_dx + p_sw - 1 do
            draw_pixel(dst, dx, dy, get_pixel(src, sx, sy))
            sx = sx + 1
        end
        sy = sy + 1
    end
end

local function process_sprite(spr)
    local filename = string.match(spr.filename, "([^/\\]*)%..-$")
    assert(filename, "could not extract path filename")

    local prefix = string.upper(filename)
    local frame_data = {}

    -- write frame data
    for fi=1, #spr.frames do
        local cel = spr.cels[fi]

        -- if this frame is a duplicate, continue
        local dup_of_frame = nil
        for fj=1, fi-1 do
            local cel2 = spr.cels[fj]
            if cel.bounds == cel2.bounds and
               (cel.image == cel2.image or cel.image:isEqual(cel2.image))
            then
                dup_of_frame = fj
                break
            end
        end

        if dup_of_frame then
            frame_data[fi] = frame_data[dup_of_frame]
            goto continue
        end
        
        local bounds = cel.bounds
        local bx, by = bounds.x, bounds.y
        local bw, bh = bounds.width, bounds.height
        local src = cel.image
        local sc = math.floor(bx / 4)
        local sr = math.floor(by / 4)
        local ec = math.ceil((bx + bw) / 4) - 1
        local er = math.ceil((by + bh) / 4) - 1

        frame_data[fi] = {
            idx = out_idx,
            sx = sc,
            sy = sr,
            w = ec - sc + 1,
            h = er - sr + 1
        }

        local sy = 0
        for row=sr, er do
            local oy = math.min(0, row * 4 - by)
            local sh = math.min(bh, sy + 4 + oy) - sy

            local sx = 0
            for col=sc, ec do
                local ox = math.min(0, col * 4 - bx)
                local sw = math.min(bw, sx + 4 + ox) - sx

                local output_x = (out_idx % 32) * 4
                local output_y = (out_idx // 32) * 4

                img_blit(output, src, output_x - ox, output_y - oy, sx, sy, sw,
                         sh)
                
                sx = sx + sw

                out_idx = out_idx + 1
            end

            sy = sy + sh
        end

        ::continue::
    end

    local function insert_frame(frame_list, f)
        local fdat = frame_data[f]
        table.insert(frame_list, {
            idx = fdat.idx,
            w = fdat.w,
            h = fdat.h,
            len = math.floor(spr.frames[f].duration * 60.0 + 0.5),
            ox = fdat.sx * 8,
            oy = fdat.sy * 8
        })
    end

    -- write animation data
    if spr.tags[1] then
        for _, tag in ipairs(spr.tags) do
            local gfx_name = prefix .. "_" .. string.upper(tag.name)
            if string.match(gfx_name, "[^A-Z_]") then
                error("invalid name " .. gfx_name)
            end

            local anim = {}
            anim.name = gfx_name
            anim.loop = tag.repeats == 0
            anim.frames = {}

            for f=tag.fromFrame.frameNumber, tag.toFrame.frameNumber do
                insert_frame(anim.frames, f)
            end

            table.insert(output_data, anim)
        end
    else
        local anim = {}
        anim.name = prefix
        anim.loop = false
        anim.frames = {}

        for i=1, #spr.frames do
            insert_frame(anim.frames, i)
        end

        table.insert(output_data, anim)
    end
end

local function process_file(file_name)
    local s, err = pcall
    local spr = app.open(file_name)
    if not spr then
        error("error opening " .. file_name)
    end

    spr:setPalette(pico8_pal)
    app.command.ChangePixelFormat { format = "indexed" }
    app.command.FlattenLayers { visibleOnly = true }
    process_sprite(spr)
    spr:close()
end

-- sorted greedily
local SPRITE_SIZES = {
    {8, 8, ATTR0_SQUARE, ATTR1_SIZE_64},
    {8, 4, ATTR0_WIDE,   ATTR1_SIZE_64},
    {4, 8, ATTR0_TALL,   ATTR1_SIZE_64},

    {4, 4, ATTR0_SQUARE, ATTR1_SIZE_32},
    {4, 2, ATTR0_WIDE,   ATTR1_SIZE_32},
    {2, 4, ATTR0_TALL,   ATTR1_SIZE_32},

    {4, 1, ATTR0_WIDE,   ATTR1_SIZE_16},
    {1, 4, ATTR0_TALL,   ATTR1_SIZE_16},
    {2, 2, ATTR0_SQUARE, ATTR1_SIZE_16},

    {2, 1, ATTR0_WIDE,   ATTR1_SIZE_8 },
    {1, 2, ATTR0_TALL,   ATTR1_SIZE_8 },
    {1, 1, ATTR0_SQUARE, ATTR1_SIZE_8 },
}

local function objdef(a0, a1, char, ox, oy)
    return string.pack("<!4 I2 I2 I2 i1 i1", a0, a1, char & 0x1FF, ox, oy)
end

local function create_objs(out, frame)
    local w, h = frame.w, frame.h
    local count = 0
    local char_ofs = 0

    for suby=0, h-1 do
        for _, size in ipairs(SPRITE_SIZES) do
            local sw, sh, a0, a1 = size[1], size[2], size[3], size[4]
            if sw == w and sh == h - suby then
                local ox = frame.ox
                local oy = frame.oy + suby * 8
                local data = objdef(a0, a1, frame.idx + char_ofs, ox, oy)
                table.insert(out, data)
                count = count + 1
                goto done
            end
        end

        local subx = 0
        while subx < w do
            local ox = frame.ox + subx * 8
            local oy = frame.oy + suby * 8
            local sw = w - subx

            local a0, a1, size
            if sw >= 4 then
                a0 = ATTR0_WIDE
                a1 = ATTR1_SIZE_16
                size = 4
            elseif sw >= 2 then
                a0 = ATTR0_WIDE
                a1 = ATTR1_SIZE_8
                size = 2
            else
                a0 = ATTR0_SQUARE
                a1 = ATTR1_SIZE_8
                size = 1
            end

            local obj = objdef(a0, a1, frame.idx + char_ofs, ox, oy)
            table.insert(out, obj)

            count = count + 1
            char_ofs = char_ofs + size
            subx = subx + size
        end
    end

    ::done::
    return count
end

local function parse_sprdb(path)
    local sprdb
    do
        local f <close>, err = io.open(path, "r")
        if not f then
            error(("could not open '%s': %s"):format(path, err))
        end

        sprdb = json.decode(f:read("a"))
    end

    local files = {}
    for _, f in ipairs(sprdb.sprites) do
        table.insert(files, app.fs.normalizePath(app.fs.joinPath(app.fs.currentPath, sprdb.dir, f)))
    end

    return files
end

local function process_sprdb(output_img, output_dat, output_h)
    for _, file_name in ipairs(input_file_list) do
        process_file(file_name)
    end

    output_sprite.cels[1].image = output
    app.sprite = output_sprite
    app.command.SpriteSize { ui = false, scale = 2.0 }
    output_sprite:saveAs(output_img)

    local output_dat_f <close>, err = io.open(output_dat, "wb")
    if not output_dat_f then
        error(("could not open '%s': %s"):format(app.params.output_dat, err))
    end

    local output_h_f <close>, err = io.open(output_h, "w")
    if not output_h_f then
        error(("could not open '%s': %s"):format(output_h, err))
    end

    local spack = string.pack

    local names = {}

    local frame_pool_buf = {}
    local obj_pool_buf = {}
    local gfx_data_buf = {}

    for _, anim in ipairs(output_data) do
        tinsert(names, anim.name)
        tinsert(gfx_data_buf, spack("<!4 I1 I1 I2", #anim.frames, anim.loop and 1 or 0, #frame_pool_buf))

        for _, frame in ipairs(anim.frames) do
            local obji = #obj_pool_buf
            local objc = create_objs(obj_pool_buf, frame)

            tinsert(frame_pool_buf,
                    spack("<!4 I2 I1 I1", obji, frame.len, objc))
        end
    end

    local gfx_data = table.concat(gfx_data_buf)
    local frame_pool = table.concat(frame_pool_buf)
    local obj_pool = table.concat(obj_pool_buf)

    local sizeof_gfx_data = string.len(gfx_data)
    local sizeof_frame_pool = string.len(frame_pool)
    local sizeof_obj_pool = string.len(obj_pool)

    assert(sizeof_gfx_data % 4 == 0)
    assert(sizeof_frame_pool % 4 == 0)
    assert(sizeof_obj_pool % 4 == 0)
    
    local data = {}
    tinsert(data, spack("<!4 I4 I4", 8 + sizeof_gfx_data, 8 + sizeof_gfx_data + sizeof_frame_pool))
    tinsert(data, gfx_data)
    tinsert(data, frame_pool)
    tinsert(data, obj_pool)

    output_dat_f:write(table.concat(data))
    -- print(json.encode(output_data))

    local h_name = string.match(output_h, "([^/\\]+)%..-$")
    assert(h_name)

    local sprdb_ident = string.gsub(string.match(h_name, "(.*)_sprdb$"), "[^A-Za-z_]", "_")
    assert(sprdb_ident)

    local inc_guard_name = string.upper(h_name) .. "_H"

    output_h_f:write("#ifndef ", inc_guard_name, "\n")
    output_h_f:write("#define ", inc_guard_name, "\n\n")

    output_h_f:write("#include <", h_name, "_data.h", ">\n")
    output_h_f:write("#include <", h_name, "_gfx.h", ">\n\n")

    output_h_f:write("typedef enum sprid_", sprdb_ident, "\n{\n")

    for _, name in ipairs(names) do
        output_h_f:write("    ")
        output_h_f:write("SPRID_" .. string.upper(sprdb_ident) .. "_" .. name)
        output_h_f:write(",\n")
    end

    output_h_f:write("    GFXID_COUNT\n")
    output_h_f:write("} sprid_", sprdb_ident, "_e;\n")

    output_h_f:write("\n#endif\n")
end

do
    if not app.params.sprdb then
        error("expected 'sprdb' parameter")
    end

    local sprdb_file = app.fs.normalizePath(app.params.sprdb)

    input_file_list = parse_sprdb(sprdb_file)

    if app.params.out_dep then
        if not app.params.artifact_name then
            error("expected 'artifact_name' parameter")
        end

        local f <close>, err = io.open(app.params.out_dep, "w")
        if not f then
            error(("could not open '%s': %s"):format(f, err))
        end

        f:write(app.params.artifact_name)
        f:write(": ")
        f:write(sprdb_file)

        for i, path in ipairs(input_file_list) do
            f:write(" \\\n ")
            f:write((string.gsub(path, "\\", "/")))
        end

        f:write("\n")

        for _, path in ipairs(input_file_list) do
            f:write((string.gsub(path, "\\", "/")))
            f:write(":\n")
        end
    end

    if not app.params.output_img then
        error("expected 'output_img' parameter")
    end

    if not app.params.output_dat then
        error("expected 'output_dat' parameter")
    end

    if not app.params.output_h then
        error("expected 'output_h' parameter")
    end

    process_sprdb(app.params.output_img, app.params.output_dat,
                    app.params.output_h)
end