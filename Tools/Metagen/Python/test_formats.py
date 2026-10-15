import copy
import hashlib
import random
import struct
import unittest

from data_formats import (CODECS, binary_decode, binary_encode, json_decode, json_encode,
                          pack, unpack, xml_decode, xml_encode)
from metagen_source import SchemaError
from test_data import document

DIGEST = hashlib.sha256(b"format conformance fixture").digest()


class FormatTests(unittest.TestCase):
    def test_authored_round_trips_and_canonical_order(self):
        tree = document()
        tree["objects"][0]["name"] = "UTF-8 snowman \u2603 < & \"\n\t\x01"
        for encode, decode in ((json_encode, json_decode), (xml_encode, xml_decode),
                               (lambda t: binary_encode(t, DIGEST), lambda b: binary_decode(b, DIGEST))):
            encoded = encode(tree)
            self.assertEqual(decode(encoded), tree)
            reverse = dict(reversed(list(tree.items())))
            self.assertEqual(encode(reverse), encoded)
            self.assertEqual(encode(decode(encoded)), encoded)

    def test_all_default_compression_codecs(self):
        for encoded in (json_encode(document()), xml_encode(document()), binary_encode(document(), DIGEST)):
            for codec in CODECS:
                with self.subTest(codec=codec, size=len(encoded)):
                    wrapped = pack(encoded, codec)
                    self.assertEqual(unpack(wrapped), encoded)
                    flipped = wrapped[:49] + bytes([wrapped[49] ^ 1]) + wrapped[50:]
                    for corrupt in (wrapped[:-1], wrapped + b"x", flipped):
                        with self.assertRaises(SchemaError):
                            unpack(corrupt)
                    # A truthful encoded digest with a lying expansion length must still fail boundedly.
                    bad = bytearray(wrapped)
                    struct.pack_into("<I", bad, 5, 1)
                    with self.assertRaises(SchemaError):
                        unpack(bytes(bad))

    def test_empty_codec_payloads(self):
        for codec in CODECS:
            with self.subTest(codec=codec):
                self.assertEqual(unpack(pack(b"", codec)), b"")

    def test_json_rejections(self):
        for data in (b'{"a": 1, "a": 2}', b'{"a": NaN}', b'{"a": 1e999}', b'"\xff"', b'"\\ud800"', b'"\\u0000"'):
            with self.subTest(data=data), self.assertRaises(SchemaError):
                json_decode(data)

    def test_xml_rejections(self):
        cases = [b'<!DOCTYPE x [<!ENTITY e "boom">]><metagen version="1"><string>&e;</string></metagen>',
                 b'<?arbitrary x?><metagen version="1"><null/></metagen>',
                 b'<metagen version="1"><object><entry key="&quot;a&quot;"><null/></entry><entry key="&quot;a&quot;"><null/></entry></object></metagen>',
                 b'<metagen version="1"><list bad="x"/></metagen>', b'<metagen version="1"><bool>1</bool></metagen>',
                 b'<metagen version="1"><null/>extra</metagen>', b'<metagen version="1"><unknown/></metagen>',
                 '<!DOCTYPE x [<!ENTITY e "boom">]><metagen version="1"><null/></metagen>'.encode('utf-16')]
        for data in cases:
            with self.subTest(data=data), self.assertRaises(SchemaError):
                xml_decode(data)

    def test_binary_scalar_boundaries(self):
        data = [None, False, True, -2**63, -1, 0, 2**64-1, -0.0, 1.25, "", "\u2603", {}, []]
        self.assertEqual(binary_decode(binary_encode(data, DIGEST), DIGEST), data)

    def test_binary_rejections_and_every_truncated_prefix(self):
        encoded = binary_encode(document(), DIGEST)
        for size in range(len(encoded)):
            with self.subTest(size=size), self.assertRaises(SchemaError):
                binary_decode(encoded[:size], DIGEST)
        for data in (encoded + b'x', b'MDB1' + bytes(32) + encoded[36:],
                     b'MDB1' + DIGEST + b'\xff', b'MDB1' + DIGEST + b'\x03\x80\0',
                     b'MDB1' + DIGEST + b'\x03' + b'\xff' * 10,
                     b'MDB1' + DIGEST + b'\x05' + struct.pack('<d', float('nan'))):
            with self.subTest(data=data[:40]), self.assertRaises(SchemaError):
                binary_decode(data, DIGEST)

    def test_deterministic_garbage_corpus(self):
        rng = random.Random(9123)
        for _ in range(500):
            garbage = rng.randbytes(rng.randrange(128))
            for decode in (json_decode, xml_decode, lambda b: binary_decode(b, DIGEST), unpack):
                try:
                    decode(garbage)
                except SchemaError:
                    pass


if __name__ == '__main__':
    unittest.main()
