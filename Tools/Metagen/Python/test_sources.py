import copy
from pathlib import Path
import tempfile
import unittest

from metagen_source import read, SchemaError
from portable_schema import normalize, fingerprint

FIXTURE = Path(__file__).resolve().parents[1] / "Fixtures/paddle_schema.py"


class SchemaTests(unittest.TestCase):
    def setUp(self):
        self.source = FIXTURE.read_text()
        self.schema = read(FIXTURE)["schema"]

    def parse(self, text):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "schema.py"
            path.write_text(text)
            return normalize(read(path)["schema"])

    def rejects(self, mutate):
        candidate = copy.deepcopy(self.schema)
        mutate(candidate)
        with self.assertRaises(SchemaError):
            normalize(candidate)

    def test_defaults_and_types(self):
        schema = normalize(self.schema)[0]
        self.assertEqual(schema["fields"][0]["type"]["kind"], "float32")
        self.assertAlmostEqual(schema["fields"][0]["default"], 2.8, places=6)
        self.assertIsNone(schema["fields"][4]["default"])
        self.assertTrue(schema["fields"][4]["has_default"])
        self.assertEqual(schema["fields"][3]["tags"]["editor.label"], "Player name")

    def test_no_import_or_execution(self):
        with tempfile.TemporaryDirectory() as directory:
            marker = Path(directory) / "executed"
            for payload in [f"open({str(marker)!r}, 'w').write('bad')\n", "raise Exception('executed')\n"]:
                with self.assertRaises(SchemaError):
                    self.parse(payload + self.source)
                self.assertFalse(marker.exists())
        for expression in ["__import__('os').getcwd()", "lambda: 3", "dict(id=1)"]:
            with self.subTest(expression=expression), self.assertRaises(SchemaError):
                self.parse(self.source.replace('default=2.8', f'default={expression}'))

    def test_arbitrary_annotations_and_factories_rejected(self):
        for old, new in [("Annotated[float, f32]", "float"), ("default=2.8", "default_factory=lambda: 2.8"),
                         ("@dataclass", "@unknown()"), ("class PaddleSettings:", "class PaddleSettings(object):")]:
            with self.subTest(new=new), self.assertRaises(SchemaError):
                self.parse(self.source.replace(old, new))

    def test_invalid_identities_and_types(self):
        for value in [0, -1, True, 2**32, "1"]:
            self.rejects(lambda s: s[0]["fields"][0].update(id=value))
        self.rejects(lambda s: s[0]["fields"][1].update(id=1))
        self.rejects(lambda s: s[0]["fields"][1].update(name="maximum_speed"))
        self.rejects(lambda s: s[0]["fields"][0].update(name="class"))
        self.rejects(lambda s: s[0].update(version=True))
        self.rejects(lambda s: s.append(copy.deepcopy(s[0])))
        self.rejects(lambda s: s[0]["fields"][0]["type"].update(kind="object"))

    def test_invalid_defaults(self):
        for value in [None, True, 1e100, float("nan"), float("inf"), -1]:
            self.rejects(lambda s: s[0]["fields"][0].update(default=value))
        for value in [-1, 2**32, 2.5, True]:
            self.rejects(lambda s: s[0]["fields"][2].update(default=value))
        self.rejects(lambda s: s[0]["fields"][1].update(default=1))
        self.rejects(lambda s: s[0]["fields"][3].update(default="x" * 4097))
        self.rejects(lambda s: s[0]["fields"][3].update(default="a\0b"))
        self.rejects(lambda s: s[0]["fields"][3].update(default="\ud800"))
        self.rejects(lambda s: s[0]["fields"][0].update(minimum=30))
        self.rejects(lambda s: s[0]["fields"][1].update(minimum=0))

    def test_missing_null_and_order(self):
        missing = copy.deepcopy(self.schema)
        missing[0]["fields"][4]["has_default"] = False
        self.assertNotEqual(fingerprint(normalize(missing)), fingerprint(normalize(self.schema)))
        reordered = copy.deepcopy(self.schema)
        reordered[0]["fields"].reverse()
        self.assertEqual(fingerprint(normalize(reordered)), fingerprint(normalize(self.schema)))

    def test_malformed_projection_and_generated_names(self):
        self.rejects(lambda s: s.__setitem__(0, None))
        self.rejects(lambda s: s[0]["fields"].__setitem__(0, None))
        self.rejects(lambda s: s[0]["fields"][0]["type"].update(kind=[]))
        self.rejects(lambda s: s[0].update(tags={1: "bad", "x": "bad"}))
        self.rejects(lambda s: s[0]["fields"][0].update(default=10**500))
        self.rejects(lambda s: s[0]["fields"][0].update(minimum=10**500))
        self.rejects(lambda s: s[0]["fields"][0].update(name="paddle_settings"))
        self.rejects(lambda s: s[0].update(name="to_record"))
        self.rejects(lambda s: s[0]["fields"][0].update(tags={"metagen.field_id": "spoof"}))

    def test_float64_and_required_field(self):
        candidate = self.source.replace("Annotated[float, f32]", "Annotated[float, f64]").replace("f32, i32, u32", "f64, i32, u32")
        result = self.parse(candidate)[0]["fields"][0]
        self.assertEqual(result["type"]["kind"], "float64")
        self.assertEqual(result["default"], 2.8)
        required = self.parse(self.source.replace("default=True, ", ""))[0]["fields"][1]
        self.assertFalse(required["has_default"])


if __name__ == "__main__":
    unittest.main()
