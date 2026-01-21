import os
import random
import struct
import json
import uuid
import math

def _clamp_channel(value):
    if value < 0:
        return 0
    if value > 255:
        return 255
    return int(value)

def _clamp_color(color):
    return (_clamp_channel(color[0]), _clamp_channel(color[1]), _clamp_channel(color[2]))

def _hash_noise(x, y, seed):
    n = x * 374761393 + y * 668265263 + seed * 700001
    n = (n ^ (n >> 13)) * 1274126177
    n = n ^ (n >> 16)
    return (n & 0xFFFFFFFF) / 0xFFFFFFFF

def _noise_signed(x, y, seed):
    return _hash_noise(x, y, seed) * 2.0 - 1.0

def generate_asset_json(texture_filename):
    asset_filename = texture_filename + ".asset.json"
    print(f"Generating asset file {asset_filename}...")
    
    relative_source = os.path.relpath(texture_filename, "assets").replace("\\", "/")
    
    asset_data = {
        "version": 1,
        "id": str(uuid.uuid4()),
        "type": "Texture",
        "source": relative_source
    }
    
    with open(asset_filename, 'w') as f:
        json.dump(asset_data, f, indent=2)

def write_png(filename, width, height, pixels):
    # This is a very basic PNG writer to avoid external dependencies like Pillow.
    # It supports truecolor (RGB) without alpha.
    import zlib

    def png_pack(png_tag, data):
        chunk_head = png_tag + data
        return (struct.pack("!I", len(data)) +
                chunk_head +
                struct.pack("!I", 0xFFFFFFFF & zlib.crc32(chunk_head)))

    raw_data = b""
    for y in range(height):
        raw_data += b"\0" # Filter type 0 (None)
        for x in range(width):
            r, g, b = pixels[y * width + x]
            raw_data += struct.pack("BBB", r, g, b)

    compressed_data = zlib.compress(raw_data)

    with open(filename, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(png_pack(b"IHDR", struct.pack("!IIBBBBB", width, height, 8, 2, 0, 0, 0))) # 8-bit, truecolor
        f.write(png_pack(b"IDAT", compressed_data))
        f.write(png_pack(b"IEND", b""))

def generate_checkerboard(filename, width=256, height=256, cell_size=32, color1=(255, 255, 255), color2=(0, 0, 0)):
    print(f"Generating {filename}...")
    pixels = []
    for y in range(height):
        for x in range(width):
            if ((x // cell_size) + (y // cell_size)) % 2 == 0:
                pixels.append(color1)
            else:
                pixels.append(color2)
    
    write_png(filename, width, height, pixels)
    print(f"Done: {filename}")
    generate_asset_json(filename)

def generate_noise(filename, width=256, height=256):
    print(f"Generating {filename}...")
    pixels = []
    for y in range(height):
        for x in range(width):
            val = random.randint(0, 255)
            pixels.append((val, val, val))
    
    write_png(filename, width, height, pixels)
    print(f"Done: {filename}")
    generate_asset_json(filename)

def generate_flat_color(filename, width=256, height=256, color=(128, 128, 128)):
    print(f"Generating {filename}...")
    pixels = [color] * (width * height)
    write_png(filename, width, height, pixels)
    print(f"Done: {filename}")
    generate_asset_json(filename)

def generate_brick_albedo(filename, width=512, height=512, brick_width=64, brick_height=32,
                          mortar=4, brick_color=(156, 58, 44), mortar_color=(200, 200, 200),
                          seed=11):
    print(f"Generating {filename}...")
    pixels = []
    for y in range(height):
        row = y // brick_height
        y_in = y % brick_height
        row_offset = (brick_width // 2) if (row % 2) else 0
        for x in range(width):
            x_shifted = (x + row_offset) % width
            col = x_shifted // brick_width
            x_in = x_shifted % brick_width

            if (x_in < mortar or x_in >= brick_width - mortar or
                y_in < mortar or y_in >= brick_height - mortar):
                color = mortar_color
            else:
                brick_variation = _noise_signed(col, row, seed) * 18
                pixel_variation = _noise_signed(x, y, seed + 3) * 6
                color = _clamp_color((
                    brick_color[0] + brick_variation + pixel_variation,
                    brick_color[1] + brick_variation * 0.7 + pixel_variation,
                    brick_color[2] + brick_variation * 0.5 + pixel_variation,
                ))
            pixels.append(color)

    write_png(filename, width, height, pixels)
    print(f"Done: {filename}")
    generate_asset_json(filename)

def generate_wood_planks_albedo(filename, width=512, height=512, plank_width=96, gap=4,
                                base_color=(162, 110, 63), seed=21):
    print(f"Generating {filename}...")
    pixels = []
    for y in range(height):
        y_norm = y / height
        for x in range(width):
            plank = x // plank_width
            x_in = x % plank_width
            if x_in < gap:
                color = (82, 60, 40)
            else:
                t = x_in / plank_width
                grain = math.sin((y_norm * 12.0 + _hash_noise(plank, 0, seed) * 2.0) * math.pi * 2.0)
                rings = math.sin((t * 3.0 + y_norm * 1.5 + _hash_noise(plank, 1, seed) * 0.5)
                                 * math.pi * 2.0)
                noise = _noise_signed(x, y, seed + 5)
                tone = (grain * 0.6 + rings * 0.4) * 10 + noise * 6
                color = _clamp_color((
                    base_color[0] + tone,
                    base_color[1] + tone * 0.8,
                    base_color[2] + tone * 0.6,
                ))
            pixels.append(color)

    write_png(filename, width, height, pixels)
    print(f"Done: {filename}")
    generate_asset_json(filename)

def generate_grass_albedo(filename, width=512, height=512, base_color=(60, 120, 50), seed=31):
    print(f"Generating {filename}...")
    pixels = []
    for y in range(height):
        for x in range(width):
            noise = _noise_signed(x, y, seed)
            fine = _noise_signed(x // 2, y // 2, seed + 1)
            blade = 12 if _hash_noise(x // 4, y, seed + 2) > 0.965 else 0
            color = _clamp_color((
                base_color[0] + noise * 8 + fine * 4 + blade,
                base_color[1] + noise * 14 + fine * 6 + blade,
                base_color[2] + noise * 8 + fine * 4 + blade,
            ))
            pixels.append(color)

    write_png(filename, width, height, pixels)
    print(f"Done: {filename}")
    generate_asset_json(filename)

def generate_sand_albedo(filename, width=512, height=512, base_color=(194, 178, 128), seed=41):
    print(f"Generating {filename}...")
    pixels = []
    for y in range(height):
        for x in range(width):
            noise = _noise_signed(x, y, seed)
            speck = -20 if _hash_noise(x, y, seed + 1) > 0.995 else 0
            color = _clamp_color((
                base_color[0] + noise * 10 + speck,
                base_color[1] + noise * 8 + speck,
                base_color[2] + noise * 6 + speck,
            ))
            pixels.append(color)

    write_png(filename, width, height, pixels)
    print(f"Done: {filename}")
    generate_asset_json(filename)

def generate_painted_metal_albedo(filename, width=512, height=512,
                                  base_color=(80, 120, 140), seed=51):
    print(f"Generating {filename}...")
    pixels = []
    for y in range(height):
        for x in range(width):
            noise = _noise_signed(x, y, seed)
            scratch = 18 if _hash_noise(x, y // 6, seed + 1) > 0.997 else 0
            chip = -25 if _hash_noise(x // 4, y // 4, seed + 2) > 0.997 else 0
            color = _clamp_color((
                base_color[0] + noise * 6 + scratch + chip,
                base_color[1] + noise * 8 + scratch + chip,
                base_color[2] + noise * 10 + scratch + chip,
            ))
            pixels.append(color)

    write_png(filename, width, height, pixels)
    print(f"Done: {filename}")
    generate_asset_json(filename)

if __name__ == "__main__":
    os.makedirs("assets/textures", exist_ok=True)
    generate_checkerboard("assets/textures/checkerboard.png")
    generate_noise("assets/textures/noise.png")
    generate_flat_color("assets/textures/red.png", color=(255, 0, 0))
    generate_flat_color("assets/textures/green.png", color=(0, 255, 0))
    generate_flat_color("assets/textures/blue.png", color=(0, 0, 255))
    generate_brick_albedo("assets/textures/albedo_brick_red.png")
    generate_wood_planks_albedo("assets/textures/albedo_wood_planks.png")
    generate_grass_albedo("assets/textures/albedo_grass.png")
    generate_sand_albedo("assets/textures/albedo_sand.png")
    generate_painted_metal_albedo("assets/textures/albedo_painted_metal.png")
