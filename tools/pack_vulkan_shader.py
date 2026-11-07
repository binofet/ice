"""Pack two offline Vulkan1.1 SPIR-V modules into shader_snapshot's text contract."""
import argparse
from pathlib import Path
import struct


def pack(vertex, fragment):
    parts = ["ice-vulkan-spirv-v1"]
    for name, data in (("vertex", vertex), ("fragment", fragment)):
        if len(data) % 4 or not 20 <= len(data) <= 32768 * 4:
            raise ValueError("SPIR-V stage must contain 5..32768 complete words")
        words = struct.unpack("<" + "I" * (len(data) // 4), data)
        if words[0] != 0x07230203 or not 0x00010000 <= words[1] <= 0x00010300 or words[1] & 255:
            raise ValueError("Expected little-endian Vulkan1.1 SPIR-V module")
        parts += [name + " " + str(len(words)), " ".join(format(word, "08x") for word in words)]
    result = "\n".join(parts) + "\n"
    if len(result) > 1024 * 1024:
        raise ValueError("Shader packet exceeds shader_snapshot's 1MiB limit")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("vertex", type=Path)
    parser.add_argument("fragment", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    # This is an explicit build producer for its named shader packet, never a runtime compiler.
    def read(path):
        with path.open("rb") as stream:
            return stream.read(32768 * 4 + 1)
    result = pack(read(args.vertex), read(args.fragment))
    args.output.write_text(result, encoding="ascii")


if __name__ == "__main__":
    main()
