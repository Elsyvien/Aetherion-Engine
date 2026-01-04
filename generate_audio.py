import wave
import math
import struct
import os
import json
import uuid

def generate_asset_json(audio_filename):
    asset_filename = audio_filename + ".asset.json"
    print(f"Generating asset file {asset_filename}...")
    
    # Calculate relative path from assets directory
    # Assuming audio_filename starts with "assets/"
    relative_source = os.path.relpath(audio_filename, "assets").replace("\\", "/")
    
    asset_data = {
        "version": 1,
        "id": str(uuid.uuid4()),
        "type": "Audio",
        "source": relative_source
    }
    
    with open(asset_filename, 'w') as f:
        json.dump(asset_data, f, indent=2)

def generate_sine_wave(filename, frequency=440.0, duration=1.0, sample_rate=44100, volume=0.5):
    print(f"Generating {filename}...")
    num_samples = int(duration * sample_rate)
    
    # Ensure directory exists
    os.makedirs(os.path.dirname(filename), exist_ok=True)

    with wave.open(filename, 'w') as wav_file:
        # Set parameters: nchannels, sampwidth, framerate, nframes, comptype, compname
        wav_file.setparams((1, 2, sample_rate, num_samples, 'NONE', 'not compressed'))
        
        for i in range(num_samples):
            t = float(i) / sample_rate
            value = volume * math.sin(2.0 * math.pi * frequency * t)
            # Scale to 16-bit integer range (-32768 to 32767)
            packed_value = struct.pack('<h', int(value * 32767.0))
            wav_file.writeframes(packed_value)
    print(f"Done: {filename}")
    generate_asset_json(filename)

if __name__ == "__main__":
    generate_sine_wave("assets/audio/test_sine_440.wav", frequency=440.0, duration=2.0)
    generate_sine_wave("assets/audio/test_sine_880.wav", frequency=880.0, duration=1.0)