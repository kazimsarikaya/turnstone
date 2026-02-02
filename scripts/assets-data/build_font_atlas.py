#!/usr/bin/env python3
import freetype as ft
import numpy as np
from scipy.ndimage import distance_transform_edt
from PIL import Image
import math, os
import argparse
import zlib

parser =argparse.ArgumentParser(description="Generate SDF font atlas from TTF")
parser.add_argument("--base_dir", type=str, default="..", help="Base directory")
parser.add_argument("--assert_src_dir", type=str, default="scripts/assets-data", help="Assets source directory")
parser.add_argument("--font", type=str, default="font_gui.ttf", help="Font filename")
parser.add_argument("--cc_dir", type=str, default="assets-cc-gen/graphics", help="CC directory")
parser.add_argument("--includes_dir", type=str, default="includes-gen/graphics", help="Includes directory")
parser.add_argument("--assets_dir", type=str, default="assets-gen/turnstone/kernel/hw/video", help="Assets directory")
parser.add_argument("--build_dir", type=str, default="build", help="Build directory")
parser.add_argument("--output_prefix", type=str, default="font_atlas", help="Output prefix")
parser.add_argument("--glyph_box_size", type=int, default=64, help="Glyph box size in pixels")
parser.add_argument("--spread", type=int, default=4, help="SDF spread in pixels")
parser.add_argument("--invert_png", action="store_true", help="Invert PNG output (white on black)")

args = parser.parse_args()


# ----------------------------
# Configuration
# ----------------------------
font_path = os.path.join(args.base_dir, args.assert_src_dir, args.font)
png_path = os.path.join(args.base_dir, args.build_dir, f"{args.output_prefix}.png")

cc_dir = os.path.join(args.base_dir, args.cc_dir)
os.makedirs(cc_dir, exist_ok=True)
cc_path = os.path.join(cc_dir, f"{args.output_prefix}.64.c")

includes_dir = os.path.join(args.base_dir, args.includes_dir)
os.makedirs(includes_dir, exist_ok=True)
h_path = os.path.join(includes_dir, f"{args.output_prefix}.h")

build_dir = os.path.join(args.base_dir, args.build_dir)

deflate_exec = os.path.join(build_dir, "deflate.bin")

assets_dir = os.path.join(args.base_dir, args.assets_dir)
os.makedirs(assets_dir, exist_ok=True)
asset_pre_bin_path = os.path.join(build_dir, f"{args.output_prefix}_bitmap.bin")
asset_bin_path = os.path.join(assets_dir, f"{args.output_prefix}_bitmap.bin")

glyph_box_size = args.glyph_box_size
spread = args.spread
font_size = glyph_box_size - 2 * spread
invert_png = args.invert_png

# ----------------------------
# Load font
# ----------------------------
if not os.path.exists(font_path):
    raise FileNotFoundError(f"Font not found: {font_path}")

face = ft.Face(font_path)
face.set_pixel_sizes(0, font_size)

# ----------------------------
# Collect all glyphs
# ----------------------------
glyphs = []
for cp in range(65536):
    gi = face.get_char_index(cp)
    if gi == 0:
        continue
    try:
        face.load_glyph(gi, ft.FT_LOAD_RENDER)
        bmp = face.glyph.bitmap
        if bmp.width > 0 and bmp.rows > 0:
            glyphs.append((gi, cp))
    except Exception:
        continue

if not glyphs:
    raise RuntimeError("No glyphs found in font!")

# ----------------------------
# Determine fallback glyph
# ----------------------------
fallback_cp = 0xFFFD if any(cp == 0xFFFD for _, cp in glyphs) else ord('?')
fallback_glyph = next(((gi, cp) for gi, cp in glyphs if cp == fallback_cp), glyphs[0])
print(f"Using fallback glyph U+{fallback_glyph[1]:04X} for missing (codepoint 0u)")

# ----------------------------
# Insert fallback glyph as 0u
# ----------------------------
glyphs.insert(0, (fallback_glyph[0], 0))

# ----------------------------
# Ensure space glyph exists
# ----------------------------
space_cp = ord(' ')
has_space = any(cp == space_cp for _, cp in glyphs)
if not has_space:
    # Add a synthetic space glyph with no bitmap but with nominal advance
    print("Space glyph not found, adding synthetic space glyph.")
    glyphs.insert(1, (None, space_cp))  # None indicates synthetic glyph
else:
    print("Using existing space glyph.")


num_glyphs = len(glyphs)
grid = math.ceil(math.sqrt(num_glyphs))
atlas_size = grid * glyph_box_size
print(f"Collected {num_glyphs} glyphs → atlas {atlas_size}x{atlas_size}")

atlas = np.zeros((atlas_size, atlas_size), dtype=np.float32)

# ----------------------------
# Helper: get bitmap buffer
# ----------------------------
def get_bitmap_array(bmp):
    buf = bmp.buffer
    if isinstance(buf, (bytes, bytearray)):
        data = np.frombuffer(buf, dtype=np.uint8)
    elif isinstance(buf, list):
        data = np.array(buf, dtype=np.uint8)
    else:
        raise TypeError(f"Unexpected bitmap buffer type: {type(buf)}")
    return data.reshape((bmp.rows, bmp.width))

# ----------------------------
# Build SDF for each glyph
# ----------------------------
glyph_infos = []
for idx, (gi, cp) in enumerate(glyphs):
    if gi is None:
        # Synthetic glyph (like space)
        row = idx // grid
        col = idx % grid
        y0, x0 = row * glyph_box_size, col * glyph_box_size

        u0 = x0 / atlas_size
        v0 = y0 / atlas_size
        u1 = (x0 + glyph_box_size) / atlas_size
        v1 = (y0 + glyph_box_size) / atlas_size
        adv = glyph_box_size  # nominal advance for monospace

        # Just fill atlas cell with 1.0 (empty area in SDF)
        atlas[y0:y0 + glyph_box_size, x0:x0 + glyph_box_size] = 1.0

        glyph_infos.append((cp, u0, v0, u1, v1, adv,
                            0, 0, glyph_box_size, glyph_box_size))
        continue

    face.load_glyph(gi, ft.FT_LOAD_RENDER)
    g = face.glyph
    bmp = g.bitmap
    width, height = bmp.width, bmp.rows

    # Convert bitmap to binary mask
    mask = (get_bitmap_array(bmp) > 127).astype(float)

    # Clamp width/height to glyph_box_size
    clamped_width = min(width, glyph_box_size)
    clamped_height = min(height, glyph_box_size)
    mask = mask[:clamped_height, :clamped_width]

    # Place into fixed-size box
    big = np.zeros((glyph_box_size + 2 * spread, glyph_box_size + 2 * spread), float)
    ox = max(0, min(int(spread + g.bitmap_left), big.shape[1] - clamped_width))
    oy = max(0, min(int(spread + (font_size - g.bitmap_top)), big.shape[0] - clamped_height))
    big[oy:oy + clamped_height, ox:ox + clamped_width] = mask

    # Compute SDF
    inside = distance_transform_edt(1 - big)
    outside = distance_transform_edt(big)
    sdf = np.clip(0.5 + (inside - outside) / (2 * spread), 0, 1)
    sdf_crop = sdf[spread:spread + glyph_box_size, spread:spread + glyph_box_size]

    # Place in atlas grid
    row = idx // grid
    col = idx % grid
    y0, x0 = row * glyph_box_size, col * glyph_box_size
    atlas[y0:y0 + glyph_box_size, x0:x0 + glyph_box_size] = sdf_crop

    # GlyphInfo
    u0 = x0 / atlas_size
    v0 = y0 / atlas_size
    u1 = (x0 + glyph_box_size) / atlas_size
    v1 = (y0 + glyph_box_size) / atlas_size
    adv = glyph_box_size  # monospaced, fixed width
    glyph_infos.append((cp, u0, v0, u1, v1, adv,
                        0, 0,  # ignore bearing for fixed box
                        glyph_box_size, glyph_box_size))

atlas = 1.0 - atlas  # Invert for SDF (0=inside, 1=outside)

# ----------------------------
# Save outputs
# ----------------------------
atlas.tofile(f"{asset_pre_bin_path}")
print(f"Saved atlas binary data as {asset_pre_bin_path}")

# Optionally deflate
with open(asset_pre_bin_path, "rb") as f:
    data = f.read()
compressed_data = zlib.compress(data, level=9, wbits=-15)
with open(asset_bin_path, "wb") as f:
    f.write(compressed_data)
print(f"Saved compressed atlas binary data as {asset_bin_path}")

img = (1.0 - atlas if invert_png else atlas) * 255
img = Image.fromarray(img.astype(np.uint8), "L")
img.save(f"{png_path}")
print(f"Saved atlas PNG as {png_path}")

# ----------------------------
# Generate font_atlas.h
# ----------------------------
with open(f"{h_path}", "w") as h:
    h.write("#ifndef ___FONT_ATLAS_H\n#define ___FONT_ATLAS_H\n\n")
    h.write("#include <types.h>\n\n")
    h.write(f"#define FONT_ATLAS_WIDTH {atlas_size}u\n")
    h.write(f"#define FONT_ATLAS_HEIGHT {atlas_size}u\n")
    h.write(f"#define FONT_GLYPH_SIZE {glyph_box_size}u\n")
    h.write(f"#define FONT_GLYPH_COUNT {num_glyphs}u\n\n")

    h.write("typedef struct glyph_info_t {\n")
    h.write("    uint16_t codepoint;\n")
    h.write("    float u0, v0, u1, v1;\n")
    h.write("    float advance;\n")
    h.write("    int16_t bearing_x, bearing_y;\n")
    h.write("    uint16_t width, height;\n")
    h.write("} glyph_info_t;\n\n")

    h.write("extern const glyph_info_t font_glyphs[FONT_GLYPH_COUNT];\n")
    h.write("#endif\n")

# ----------------------------
# Generate font_atlas.c
# ----------------------------
with open(f"{cc_path}", "w") as c:
    c.write(f"#include <graphics/{args.output_prefix}.h>\n\n")
    c.write('MODULE("turnstone.kernel.graphics.font");\n\n')
    c.write("const glyph_info_t font_glyphs[FONT_GLYPH_COUNT] = {\n")
    for (cp, u0, v0, u1, v1, adv, bx, by, bw, bh) in glyph_infos:
        c.write(f"  {{ {cp}u, {u0:.6f}f, {v0:.6f}f, {u1:.6f}f, {v1:.6f}f, "
                f"{adv:.2f}f, {bx}, {by}, {bw}, {bh} }},\n")
    c.write("};\n\n")

print("Generated C and header files.")

