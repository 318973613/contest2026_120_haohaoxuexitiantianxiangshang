#!/usr/bin/env python3
"""Prepare a quieter boot PCM from a WAV, without calling any TTS service.

The DShanPi boot player expects mono, 16-bit little-endian PCM at 24 kHz.
Parse the WAV chunks instead of assuming a 44-byte header. Keep the original
asset and write a separate candidate so the board can compare both signals.
"""

import argparse
from array import array
import hashlib
import json
import math
from pathlib import Path
import sys
import wave


def stats(samples):
    peak = max((abs(value) for value in samples), default=0)
    rms = math.sqrt(sum(value * value for value in samples) / len(samples))
    return {
        "peak": peak,
        "peak_dbfs": 20 * math.log10(peak / 32768) if peak else None,
        "rms": rms,
        "rms_dbfs": 20 * math.log10(rms / 32768) if rms else None,
        "near_full_scale_samples": sum(abs(value) >= 0.98 * 32768
                                       for value in samples),
    }


def prepare(source, output, gain_db=-6.0):
    source, output = Path(source), Path(output)
    if source.resolve() == output.resolve():
        raise ValueError("Keep the source WAV; choose a separate PCM output")
    if not math.isfinite(gain_db) or not -60.0 <= gain_db <= 0:
        raise ValueError("gain-db must be between -60 and 0")

    with wave.open(str(source), "rb") as wav:
        if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate(),
                wav.getcomptype()) != (1, 2, 24000, "NONE"):
            raise ValueError("Expected uncompressed mono 16-bit 24 kHz WAV")
        frames = wav.getnframes()
        raw = wav.readframes(frames)
    if frames == 0 or len(raw) != frames * 2:
        raise ValueError("Empty or truncated WAV data chunk")

    original = array("h")
    original.frombytes(raw)
    if sys.byteorder != "little":
        original.byteswap()
    gain = 10 ** (gain_db / 20)
    adjusted = array("h", (round(value * gain) for value in original))
    report = {
        "sample_rate_hz": 24000,
        "channels": 1,
        "bits_per_sample": 16,
        "frames": frames,
        "duration_seconds": frames / 24000,
        "gain_db": gain_db,
        "source_wav_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "source_pcm_sha256": hashlib.sha256(raw).hexdigest(),
        "source": stats(original),
        "candidate": stats(adjusted),
    }
    if sys.byteorder != "little":
        adjusted.byteswap()
    payload = adjusted.tobytes()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(payload)
    report["candidate_pcm_bytes"] = len(payload)
    report["candidate_pcm_sha256"] = hashlib.sha256(payload).hexdigest()
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--gain-db", type=float, default=-6.0)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    report = prepare(args.source, args.output, args.gain_db)
    rendered = json.dumps(report, indent=2, ensure_ascii=False) + "\n"
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(rendered, encoding="utf-8")
    print(rendered, end="")


if __name__ == "__main__":
    main()
