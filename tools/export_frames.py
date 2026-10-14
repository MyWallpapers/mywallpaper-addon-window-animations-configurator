"""Convert owned native framebuffers to correctly encoded, traceable PNGs."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

from PIL import Image


def export(source: Path, output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    for name in ("open-zoom", "minimize-genie", "restore-spring", "open-wobbly"):
        path = source / "transitions" / "frames" / (name + ".bmp")
        raw = path.read_bytes()
        offset = struct.unpack_from("<I", raw, 10)[0]
        width, height = struct.unpack_from("<ii", raw, 18)
        bpp, compression = struct.unpack_from("<HI", raw, 28)
        if raw[:2] != b"BM" or width <= 0 or not height or bpp != 32 or compression:
            raise ValueError(f"Unsupported native framebuffer: {path}")
        size = width * abs(height) * 4
        pixels = bytearray(raw[offset:offset + size])
        if len(pixels) != size:
            raise ValueError(f"Truncated native framebuffer: {path}")
        for p in range(0, size, 4):
            alpha = pixels[p + 3]
            for channel in range(3):
                value = pixels[p + channel]
                if value > alpha:
                    raise ValueError(f"Source is not premultiplied: {path}")
                pixels[p + channel] = (value * 255 + alpha // 2) // alpha if alpha else 0
        if height > 0:
            stride = width * 4
            pixels = b"".join(pixels[row * stride:(row + 1) * stride]
                              for row in range(height - 1, -1, -1))
        png = output / (name + ".png")
        Image.frombytes("RGBA", (width, abs(height)), bytes(pixels), "raw", "BGRA").save(png)
        metadata = {
            "kind": "native runtime framebuffer, not a sidebar screenshot",
            "source": f"tests/runtime.ps1 / {source.name} / transitions/frames/{name}.bmp",
            "sourceSha256": hashlib.sha256(raw).hexdigest(),
            "pngSha256": hashlib.sha256(png.read_bytes()).hexdigest(),
            "minimumProgress": 0.38,
            "dimensions": [width, abs(height)],
            "windowsBuild": "26200",
            "dpi": 96,
            "transformation": "BGRA premultiplied BMP to straight-alpha RGBA PNG; rounded channel unpremultiplication",
        }
        (output / (name + ".json")).write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        print(f"Exported {name}: {width} x {abs(height)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Evidence directory from the owned Windows fixture")
    parser.add_argument("output", type=Path, help="PNG and provenance destination")
    args = parser.parse_args()
    export(args.source, args.output)
