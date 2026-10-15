"""Independent JSON/XML/binary and compression consumers of portable authored data.

MDB1 is an experimental canonical tagged-tree format, not a promised shipping asset ABI.
It preserves authored prototype/edit operations; it is not a native-layout memory dump.
"""
import hashlib
import json
import struct
import xml.etree.ElementTree as ET
import zlib

from data_model import MAX_DEPTH, MAX_ITEMS, MAX_NODES, bounded_tree
from metagen_source import SchemaError
from portable_schema import text

MAX_BYTES = 1_048_576
CODECS = ("none", "deflate", "zlib", "gzip", "bzip2", "xz", "zstd", "lz4", "brotli", "snappy")


def checked_bytes(data):
    if not isinstance(data, bytes) or len(data) > MAX_BYTES:
        raise SchemaError("Expected bounded bytes")
    return data


def json_encode(tree):
    tree = bounded_tree(tree)
    return checked_bytes((json.dumps(tree, ensure_ascii=True, allow_nan=False, sort_keys=True, indent=2) + "\n").encode())


def json_decode(data):
    checked_bytes(data)
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise SchemaError("Duplicate JSON key")
            result[key] = value
        return result
    def constant(_):
        raise SchemaError("Nonfinite JSON number")
    try:
        value = json.loads(data.decode("utf-8"), object_pairs_hook=pairs, parse_constant=constant)
        return bounded_tree(value)
    except (ValueError, UnicodeError, RecursionError) as error:
        raise SchemaError("Invalid JSON document") from error


def xml_encode(tree):
    tree = bounded_tree(tree)
    def element(value):
        if value is None:
            return ET.Element("null")
        if type(value) in (bool, int, float, str):
            kind = {bool: "bool", int: "int", float: "float", str: "string"}[type(value)]
            item = ET.Element(kind)
            # JSON escaping makes all UTF-8 strings reversible even with XML 1.0 control chars.
            item.text = json.dumps(value, ensure_ascii=True, allow_nan=False)
            return item
        item = ET.Element("object" if isinstance(value, dict) else "list")
        if isinstance(value, dict):
            for key in sorted(value):
                entry = ET.SubElement(item, "entry", key=json.dumps(key, ensure_ascii=True))
                entry.append(element(value[key]))
        else:
            for child in value:
                item.append(element(child))
        return item
    root = ET.Element("metagen", version="1")
    root.append(element(tree))
    ET.indent(root)
    return checked_bytes(ET.tostring(root, encoding="utf-8", xml_declaration=True))


def xml_decode(data):
    checked_bytes(data)
    # Reject declarations *before* feeding the parser. Comments are not a lossless editor AST.
    try:
        source = data.decode("utf-8").lstrip("\ufeff \t\r\n")
    except UnicodeError as error:
        raise SchemaError("XML input must be UTF-8") from error
    remainder = source
    if remainder.startswith("<?xml ") and "?>" in remainder:
        remainder = remainder.split("?>", 1)[1]
    if "<!" in source or "<?" in remainder or "\0" in source:
        raise SchemaError("DTD, entities and processing instructions are not permitted")
    try:
        # Feed incrementally and bound depth/count during parsing, before constructing the full tree.
        parser = ET.XMLPullParser(events=("start", "end"))
        depth = count = 0
        root = None
        for start in range(0, len(data), 1024):
            parser.feed(data[start:start + 1024])
            for event, item in parser.read_events():
                if event == "start":
                    depth += 1
                    count += 1
                    if depth > MAX_DEPTH * 2 + 4 or count > MAX_NODES * 2:
                        raise SchemaError("XML structural budget exceeded")
                    if root is None:
                        root = item
                else:
                    depth -= 1
        parser.close()
        if root is None or root.tag != "metagen" or root.attrib != {"version": "1"} or len(root) != 1:
            raise SchemaError("Invalid XML document envelope")
        def whitespace(item):
            if (item.text and item.text.strip()) or (item.tail and item.tail.strip()):
                raise SchemaError("Unexpected XML mixed content")
        def value(item, nesting=0):
            if nesting > MAX_DEPTH or item.attrib or (item.tail and item.tail.strip()):
                raise SchemaError("Invalid XML attributes/depth/mixed content")
            if item.tag in ("object", "list", "null"):
                whitespace(item)
            if item.tag == "null" and not len(item):
                return None
            if item.tag in ("bool", "int", "float", "string") and not len(item):
                result = json_decode((item.text or "").encode())
                expected = {"bool": bool, "int": int, "float": float, "string": str}[item.tag]
                if type(result) is not expected:
                    raise SchemaError("XML scalar tag/type mismatch")
                return result
            if item.tag == "list" and len(item) <= MAX_ITEMS:
                return [value(child, nesting + 1) for child in item]
            if item.tag == "object" and len(item) <= MAX_ITEMS:
                result = {}
                for entry in item:
                    whitespace(entry)
                    if entry.tag != "entry" or set(entry.attrib) != {"key"} or len(entry) != 1:
                        raise SchemaError("Invalid XML object entry")
                    key = json_decode(entry.attrib["key"].encode())
                    text(key, 4096)
                    if key in result:
                        raise SchemaError("Duplicate XML object key")
                    result[key] = value(entry[0], nesting + 1)
                return result
            raise SchemaError("Unknown XML element or invalid shape")
        whitespace(root)
        return bounded_tree(value(root[0]))
    except (ET.ParseError, ValueError, UnicodeError, RecursionError) as error:
        raise SchemaError("Invalid XML document") from error


def varint(value):
    result = bytearray()
    while value >= 128:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def binary_encode(tree, fingerprint):
    tree = bounded_tree(tree)
    if len(fingerprint) != 32:
        raise SchemaError("Expected a 32-byte schema fingerprint")
    output = bytearray(b"MDB1" + fingerprint)
    def emit(value):
        if value is None:
            output.append(0)
        elif type(value) is bool:
            output.append(2 if value else 1)
        elif type(value) is int:
            output.append(3 if value >= 0 else 4)
            output.extend(varint(value if value >= 0 else -1 - value))
        elif type(value) is float:
            output.append(5)
            output.extend(struct.pack("<d", value))
        elif type(value) is str:
            encoded = value.encode("utf-8")
            output.append(6)
            output.extend(varint(len(encoded)))
            output.extend(encoded)
        elif isinstance(value, list):
            output.append(7)
            output.extend(varint(len(value)))
            for item in value:
                emit(item)
        else:
            output.append(8)
            output.extend(varint(len(value)))
            for key in sorted(value):
                emit(key)
                emit(value[key])
        if len(output) > MAX_BYTES:
            raise SchemaError("Binary document exceeds its byte budget")
    emit(tree)
    return bytes(output)


def binary_decode(data, fingerprint):
    checked_bytes(data)
    if len(fingerprint) != 32 or data[:36] != b"MDB1" + fingerprint:
        raise SchemaError("Binary format/schema fingerprint mismatch")
    offset, remaining = 36, MAX_NODES
    def take(count):
        nonlocal offset
        if count > len(data) - offset:
            raise SchemaError("Truncated binary document")
        result = data[offset:offset + count]
        offset += count
        return result
    def uint():
        result = 0
        for index in range(10):
            byte = take(1)[0]
            if index == 9 and byte > 1:
                raise SchemaError("Varint overflow")
            result |= (byte & 127) << (index * 7)
            if byte < 128:
                if index and byte == 0:
                    raise SchemaError("Noncanonical varint")
                return result
        raise SchemaError("Invalid varint")
    def value(depth=0):
        nonlocal remaining
        remaining -= 1
        if remaining < 0 or depth > MAX_DEPTH:
            raise SchemaError("Binary node/depth budget exceeded")
        tag = take(1)[0]
        if tag <= 2:
            return (None, False, True)[tag]
        if tag in (3, 4):
            number = uint()
            if tag == 4 and number >= 2**63:
                raise SchemaError("Negative integer overflow")
            return number if tag == 3 else -1 - number
        if tag == 5:
            return struct.unpack("<d", take(8))[0]
        if tag == 6:
            size = uint()
            if size > 4096:
                raise SchemaError("Binary string budget exceeded")
            return text(take(size).decode("utf-8"), 4096)
        if tag in (7, 8):
            count = uint()
            if count > MAX_ITEMS or count > remaining:
                raise SchemaError("Binary item budget exceeded")
            if tag == 7:
                return [value(depth + 1) for _ in range(count)]
            result, previous = {}, None
            for _ in range(count):
                key = value(depth + 1)
                if not isinstance(key, str) or (previous is not None and key <= previous):
                    raise SchemaError("Binary object keys must be unique and canonical")
                previous = key
                result[key] = value(depth + 1)
            return result
        raise SchemaError("Unknown binary tag")
    try:
        result = bounded_tree(value())
        if offset != len(data):
            raise SchemaError("Trailing binary data")
        return result
    except (ValueError, UnicodeError, RecursionError) as error:
        raise SchemaError("Invalid binary document") from error


def pack(data, codec="none"):
    checked_bytes(data)
    if codec not in CODECS:
        raise SchemaError("Unknown compression codec")
    if codec == "none":
        payload = data
    else:
        import cramjam
        payload = bytes(getattr(cramjam, codec).compress(data))
    checked_bytes(payload)
    # Version, codec, declared lengths, decoded CRC, encoded digest. No format sniffing/fallback.
    return b"MCP1" + struct.pack("<BIII", CODECS.index(codec), len(data), len(payload), zlib.crc32(data)) + hashlib.sha256(payload).digest() + payload


def unpack(envelope):
    if not isinstance(envelope, bytes) or len(envelope) < 49 or len(envelope) > MAX_BYTES + 49 or envelope[:4] != b"MCP1":
        raise SchemaError("Invalid compression envelope")
    codec, size, encoded_size, checksum = struct.unpack("<BIII", envelope[4:17])
    payload = envelope[49:]
    if codec >= len(CODECS) or size > MAX_BYTES or encoded_size != len(payload) or hashlib.sha256(payload).digest() != envelope[17:49]:
        raise SchemaError("Compression envelope bounds/digest mismatch")
    if codec == 0:
        output = payload
    else:
        import cramjam
        # A fixed-size writable buffer bounds output even for a lying expansion length.
        output = bytearray(size)
        try:
            written = getattr(cramjam, CODECS[codec]).decompress_into(payload, output)
        except cramjam.DecompressionError as error:
            raise SchemaError("Invalid/oversized compressed data") from error
        if written != size:
            raise SchemaError("Decoded length mismatch")
        output = bytes(output)
    if len(output) != size or zlib.crc32(output) != checksum:
        raise SchemaError("Decoded size/checksum mismatch")
    return output
