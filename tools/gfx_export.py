#!/usr/bin/env python3

# TODO: make some frames of the player spit animation be 3 tiles tall

'''
struct gfx_data_root {
    gfx_data *gfx[?] // ? = number of graphics. pointer is relative to root.
};

struct gfx_data {    
    u8 width;
    u8 height;
    u8 frame_count;
    u8 loop; // bool

    (4 bytes padding)

    gfx_frame frames[?];
};

struct gfx_frame {
    // these attributes will be OR'd with the final object attribute.
    u16 a0; // contains only config for the sprite shape
    u16 a1; // contains only config for the sprite size
    u16 a2; // contains only config for the character index
    s8 ox;
    s8 oy;
};
'''

def main():
    pass

if __name__ == '__main__':
    main()