import os
import random
import struct
import json
import uuid

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

if __name__ == "__main__":
    os.makedirs("assets/textures", exist_ok=True)
    generate_checkerboard("assets/textures/checkerboard.png")
    generate_noise("assets/textures/noise.png")
    generate_flat_color("assets/textures/red.png", color=(255, 0, 0))
    generate_flat_color("assets/textures/green.png", color=(0, 255, 0))
    generate_flat_color("assets/textures/blue.png", color=(0, 0, 255))
