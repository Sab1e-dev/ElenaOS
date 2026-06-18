#!/usr/bin/env python3
"""Convert audio files (WAV/MP3/FLAC/OGG/...) to ElenixOS VAR (embedded PCM) C files.

Output files are compatible with eos_audio_dsc_t format:
  - A const uint8_t array with raw PCM data
  - A const eos_audio_dsc_t descriptor with EOS_AUDIO_HEADER_MAGIC

Dependencies:
  - ffmpeg (required for non-WAV formats and resampling)

Usage:
  python3 gen_var_audio.py input.wav -o output.c
  python3 gen_var_audio.py input.mp3 -o output.c -r 8000 -c 1 -b 16
  python3 gen_var_audio.py --tone 440:500:8000 -o output.c           # Generate sine tone
  python3 gen_var_audio.py --tone sweep:200-2000 --duration 1000     # Frequency sweep

The generated C file can be compiled directly into firmware.
"""

import argparse
import math
import shutil
import struct
import subprocess
import sys
import os
import tempfile


def has_ffmpeg():
    return shutil.which("ffmpeg") is not None


def run_ffmpeg(input_path, sample_rate, channels, bits):
    """Convert any audio file to raw PCM 16-bit signed LE via ffmpeg."""
    codec = {8: "pcm_u8", 16: "pcm_s16le", 24: "pcm_s24le", 32: "pcm_s32le"}[bits]
    fmt = {8: "u8", 16: "s16le", 24: "s24le", 32: "s32le"}[bits]

    with tempfile.NamedTemporaryFile(suffix=".pcm", delete=False) as tmp:
        tmp_path = tmp.name

    cmd = [
        "ffmpeg", "-y", "-v", "error",
        "-i", input_path,
        "-acodec", codec,
        "-ar", str(sample_rate),
        "-ac", str(channels),
        "-f", fmt,
        tmp_path
    ]
    subprocess.run(cmd, check=True)
    return tmp_path


def parse_raw_pcm(filepath, sample_rate, channels, bits):
    """Read raw PCM file, return (sample_rate, channels, bits_per_sample, pcm_data)."""
    with open(filepath, "rb") as f:
        pcm_data = f.read()

    if bits == 8:
        pcm_data = bytearray(pcm_data)
    elif bits == 16:
        pass
    elif bits == 24:
        samples_16 = []
        for i in range(0, len(pcm_data), 3):
            s24 = pcm_data[i] | (pcm_data[i+1] << 8) | (pcm_data[i+2] << 16)
            if s24 & 0x800000:
                s24 -= 0x1000000
            s16 = s24 >> 8
            samples_16.append(s16 & 0xFF)
            samples_16.append((s16 >> 8) & 0xFF)
        pcm_data = bytes(samples_16)
        bits = 16
    elif bits == 32:
        samples_16 = []
        for i in range(0, len(pcm_data), 4):
            s32 = struct.unpack_from("<i", pcm_data, i)[0]
            s16 = max(-32768, min(32767, s32 >> 16))
            samples_16.append(s16 & 0xFF)
            samples_16.append((s16 >> 8) & 0xFF)
        pcm_data = bytes(samples_16)
        bits = 16

    return sample_rate, channels, bits, pcm_data


def parse_wav_header(filepath):
    """Parse WAV file, return (sample_rate, channels, bits_per_sample, pcm_data)."""
    with open(filepath, "rb") as f:
        riff = f.read(4)
        if riff != b"RIFF":
            raise ValueError(f"Not a RIFF/WAV file. Use ffmpeg to convert: ffmpeg -i {filepath} output.wav")
        f.read(4)
        wave = f.read(4)
        if wave != b"WAVE":
            raise ValueError("Not a WAVE file")

        while f.read(4) != b"fmt ":
            sz = struct.unpack("<I", f.read(4))[0]
            f.read(sz)

        fmt_size = struct.unpack("<I", f.read(4))[0]
        fmt_data = f.read(fmt_size)
        audio_format = struct.unpack_from("<H", fmt_data, 0)[0]
        if audio_format != 1:
            raise ValueError(f"Compressed WAV (format={audio_format}). Use ffmpeg to decode: ffmpeg -i {filepath} output.wav")

        channels = struct.unpack_from("<H", fmt_data, 2)[0]
        sample_rate = struct.unpack_from("<I", fmt_data, 4)[0]
        bits_per_sample = struct.unpack_from("<H", fmt_data, 14)[0]

        if bits_per_sample not in (8, 16, 24, 32):
            raise ValueError(f"Unsupported bits per sample: {bits_per_sample}")

        while f.read(4) != b"data":
            sz = struct.unpack("<I", f.read(4))[0]
            f.read(sz)

        data_size = struct.unpack("<I", f.read(4))[0]
        pcm_data = f.read(data_size)

    if bits_per_sample == 8:
        pcm_data = bytearray(pcm_data)
    elif bits_per_sample == 16:
        pass
    elif bits_per_sample == 24:
        samples_16 = []
        for i in range(0, len(pcm_data), 3):
            s24 = pcm_data[i] | (pcm_data[i+1] << 8) | (pcm_data[i+2] << 16)
            if s24 & 0x800000:
                s24 -= 0x1000000
            s16 = s24 >> 8
            samples_16.append(s16 & 0xFF)
            samples_16.append((s16 >> 8) & 0xFF)
        pcm_data = bytes(samples_16)
        bits_per_sample = 16
    elif bits_per_sample == 32:
        samples_16 = []
        for i in range(0, len(pcm_data), 4):
            s32 = struct.unpack_from("<i", pcm_data, i)[0]
            s16 = max(-32768, min(32767, s32 >> 16))
            samples_16.append(s16 & 0xFF)
            samples_16.append((s16 >> 8) & 0xFF)
        pcm_data = bytes(samples_16)
        bits_per_sample = 16

    return sample_rate, channels, bits_per_sample, pcm_data


def generate_sine_tone(freq_hz, duration_ms, sample_rate):
    """Generate a sine wave tone as PCM 16-bit mono."""
    num_samples = int(sample_rate * duration_ms / 1000)
    data = bytearray()
    for i in range(num_samples):
        t = i / sample_rate
        sample = int(32767 * 0.6 * math.sin(2 * math.pi * freq_hz * t))
        data.append(sample & 0xFF)
        data.append((sample >> 8) & 0xFF)
    return sample_rate, 1, 16, bytes(data)


def generate_sweep(start_hz, end_hz, duration_ms, sample_rate):
    """Generate a linear frequency sweep as PCM 16-bit mono."""
    num_samples = int(sample_rate * duration_ms / 1000)
    data = bytearray()
    for i in range(num_samples):
        t = i / sample_rate
        progress = i / max(num_samples - 1, 1)
        freq = start_hz + (end_hz - start_hz) * progress
        phase = 2 * math.pi * freq * t
        sample = int(32767 * 0.6 * math.sin(phase))
        data.append(sample & 0xFF)
        data.append((sample >> 8) & 0xFF)
    return sample_rate, 1, 16, bytes(data)


def load_audio(path, target_rate, target_channels, target_bits):
    """Load an audio file, auto-detecting format and converting via ffmpeg if needed."""
    ext = os.path.splitext(path)[1].lower()

    if ext == ".wav" and not target_rate and not target_channels and not target_bits:
        try:
            return parse_wav_header(path)
        except ValueError as e:
            print(f"WAV parse failed: {e}")
            print("Falling back to ffmpeg...")

    if not has_ffmpeg():
        print("ERROR: ffmpeg not found. Install it: brew install ffmpeg")
        print("  Or convert to 16-bit PCM WAV: ffmpeg -i input.mp3 -acodec pcm_s16le output.wav")
        sys.exit(1)

    rate = target_rate or 8000
    channels = target_channels or 1
    bits = target_bits or 16

    pcm_path = run_ffmpeg(path, rate, channels, bits)
    try:
        result = parse_raw_pcm(pcm_path, rate, channels, bits)
    finally:
        os.unlink(pcm_path)
    return result


def generate_pcm_c(sample_rate, channels, bits_per_sample, pcm_data, var_name):
    """Generate C source code for a VAR audio descriptor."""
    total_samples = len(pcm_data) // (bits_per_sample // 8) // channels

    lines = []
    lines.append("/**")
    lines.append(f" * @file {var_name}.c")
    lines.append(f" * @brief VAR audio effect ({channels}ch, {sample_rate}Hz, {bits_per_sample}-bit)")
    lines.append(f" *")
    lines.append(f" * Duration: {total_samples * 1000 // sample_rate}ms, {total_samples} samples")
    lines.append(" */")
    lines.append("")
    lines.append('#include "eos_audio_effects.h"')
    lines.append("")
    lines.append("/* PCM data --------------------------------------------------*/")
    lines.append(f"const uint8_t {var_name}_data[] = {{")

    for offset in range(0, len(pcm_data), 16):
        chunk = pcm_data[offset:offset+16]
        hex_bytes = ", ".join(f"0x{b:02X}" for b in chunk)
        if offset + 16 < len(pcm_data):
            lines.append(f"    {hex_bytes},")
        else:
            lines.append(f"    {hex_bytes}")

    lines.append("};")
    lines.append("")
    lines.append("/* Descriptor ------------------------------------------------*/")
    lines.append(f"const eos_audio_dsc_t {var_name} = {{")
    lines.append("    .magic          = {0x1A, 'E', 'O', 'S'},")
    lines.append(f"    .sample_rate    = {sample_rate},")
    lines.append(f"    .channels       = {channels},")
    lines.append(f"    .bits_per_sample = {bits_per_sample},")
    lines.append(f"    .total_samples  = {total_samples},")
    lines.append(f"    .data_size      = {len(pcm_data)},")
    lines.append(f"    .data           = {var_name}_data,")
    lines.append("};")
    lines.append("")

    return "\n".join(lines)


def var_name_from_path(path, prefix="eos"):
    """Derive a C identifier from filename."""
    base = os.path.splitext(os.path.basename(path))[0]
    base = base.replace("-", "_").replace(" ", "_")
    base = "".join(c if c.isalnum() or c == "_" else "" for c in base)
    if not base[0].isalpha() and base[0] != "_":
        base = "_" + base
    return f"{prefix}_audio_effect_{base}"


def parse_tone_spec(spec):
    """Parse tone specification. Returns (freq_or_type, duration_ms, sample_rate)."""
    parts = spec.split(":")
    if len(parts) < 2:
        raise ValueError(
            "--tone format: FREQ:DURATION[:SAMPLE_RATE]  e.g. 440:500:8000\n"
            "  or: sweep:START-END             e.g. sweep:200-2000"
        )

    kind = parts[0].lower()
    if kind == "sweep":
        freq_range = parts[1]
        if "-" not in freq_range:
            raise ValueError("sweep format: sweep:START-END  e.g. sweep:200-2000")
        start, end = freq_range.split("-")
        return ("sweep", int(start), int(end))
    else:
        try:
            freq = int(parts[0])
            duration = int(parts[1])
            sample_rate = int(parts[2]) if len(parts) > 2 else 8000
            return ("tone", freq, duration, sample_rate)
        except ValueError:
            raise ValueError(
                "--tone expects FREQ:DURATION[:SAMPLE_RATE] (e.g. 440:500:8000)\n"
                "  or sweep:START-END (e.g. sweep:200-2000)\n"
                f"  Got: {spec}"
            )


def main():
    parser = argparse.ArgumentParser(
        description="Convert audio files to ElenixOS VAR (embedded PCM) C files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
Examples:
  %(prog)s click.wav -o eos_audio_effect_click.c
  %(prog)s music.mp3 -o eos_audio_effect_bgm.c -r 8000 -c 1 -b 16
  %(prog)s --tone 440:500:8000 -o tone_440.c
  %(prog)s --tone sweep:200-2000 --duration 1000 -r 8000 -o sweep.c
        """,
    )
    parser.add_argument("input", nargs="?", help="Input audio file (WAV/MP3/FLAC/OGG/...)")
    parser.add_argument("--tone", metavar="SPEC",
                        help="Generate tone: FREQ:DURATION[:SAMPLE_RATE] or sweep:START-END")
    parser.add_argument("-o", "--output", help="Output C file path (required)")
    parser.add_argument("-n", "--name", help="Variable name (default: derived from filename)")
    parser.add_argument("-r", "--rate", type=int, default=None,
                        help="Target sample rate in Hz (default: keep original, or 8000 for non-WAV)")
    parser.add_argument("-c", "--channels", type=int, default=None,
                        help="Target channels: 1=mono (default: keep original, or 1 for non-WAV)")
    parser.add_argument("-b", "--bits", type=int, choices=[8, 16], default=None,
                        help="Target bits per sample: 8 or 16 (default: keep original or 16)")
    parser.add_argument("--duration", type=int, default=None,
                        help="Override tone duration in ms")
    args = parser.parse_args()

    if not args.input and not args.tone:
        parser.error("either input file or --tone is required")

    if not args.output:
        parser.error("-o/--output is required")

    if args.tone:
        result = parse_tone_spec(args.input if not args.tone and args.input else args.tone)
        if result[0] == "sweep":
            _, start, end = result
            duration = args.duration or 500
            sample_rate = args.rate or 8000
            sample_rate, channels, bits, pcm = generate_sweep(start, end, duration, sample_rate)
            var_name = args.name or f"eos_audio_effect_sweep_{start}_{end}"
        else:
            _, freq, duration, sample_rate = result
            if args.duration:
                duration = args.duration
            if args.rate:
                sample_rate = args.rate
            sample_rate, channels, bits, pcm = generate_sine_tone(freq, duration, sample_rate)
            var_name = args.name or f"eos_audio_effect_tone_{freq}"
    else:
        sample_rate, channels, bits, pcm = load_audio(
            args.input, args.rate, args.channels, args.bits
        )
        var_name = args.name or var_name_from_path(args.input)

    c_code = generate_pcm_c(sample_rate, channels, bits, pcm, var_name)

    with open(args.output, "w") as f:
        f.write(c_code)

    total_samples = len(pcm) // (bits // 8) // channels
    duration_ms = total_samples * 1000 // sample_rate if sample_rate > 0 else 0
    print(f"Generated: {args.output}")
    print(f"  Variable: {var_name}")
    print(f"  Format: {channels}ch, {sample_rate}Hz, {bits}-bit")
    print(f"  Duration: {duration_ms}ms, {total_samples} samples")
    print(f"  PCM data: {len(pcm)} bytes")


if __name__ == "__main__":
    main()
