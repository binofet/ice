import copy
import unittest
from container_mappings import normalize, parse_template, render
from data_source import read
from metagen_source import SchemaError
from test_data import FIXTURE


class MappingTests(unittest.TestCase):
    def test_nested_rendering_and_order(self):
        maps = normalize(read(FIXTURE)[1])
        self.assertEqual(maps, normalize(list(reversed(maps))))
        expression = parse_template('game::map<{0}, game::vector<{1}>>', 2)
        self.assertEqual(render(expression, ['std::string', 'float']), 'game::map<std::string, game::vector<float>>')
        self.assertEqual(render(parse_template('Vec<{0}>', 1), ['Vec<u32>']), 'Vec<Vec<u32>>')

    def test_invalid_mapping_and_template_rejections(self):
        for template in ('Vec<{1}>', 'Vec<{0}>; injected()', 'Vec<{0}', 'Vec<>', 'Vec<{0}> Vec<{0}>', 'Vec<float>'):
            with self.subTest(template=template), self.assertRaises(SchemaError):
                parse_template(template, 1)
        maps = read(FIXTURE)[1]
        for mutation in (lambda m: m.update(include='../private.h'), lambda m: m.update(include='/tmp/x'),
                         lambda m: m.update(shape='multimap'), lambda m: m.update(target='guess'),
                         lambda m: m.update(adapter='thing();'), lambda m: m.update(type_template='map<{0}, {1}>')):
            bad = copy.deepcopy(maps)
            mutation(bad[0])
            with self.assertRaises(SchemaError):
                normalize(bad)
        with self.assertRaises(SchemaError):
            normalize([maps[0], maps[0]])


if __name__ == '__main__':
    unittest.main()
