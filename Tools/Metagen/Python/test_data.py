import copy
import json
from pathlib import Path
import tempfile
import unittest

from data_model import Catalog, Loader, Reference, bounded_tree
from data_source import read
from metagen_source import SchemaError

FIXTURE = Path(__file__).resolve().parents[1] / "Fixtures/party_schema.py"


def player(identity, **fields):
    return {"$type": "party.player", "id": identity, "scores": [[10, 20], []], **fields}


def document():
    return {"schemas": {"party.player": 1, "party.session": 1, "party.mode": 1}, "objects": [
        player("host", **{"$id": "host", "friend": {"$ref": "host"}}),
        {"$id": "base", "$type": "party.session", "players": [player("alice"), player("bob")],
         "settings": [["scores", [1, None, -2]]], "tags": ["pub", "party"],
         "host": {"$ref": "host"}, "guest": {"$uri": "assets://people/players.json?revision=1#guest"}},
        {"$id": "game", "$type": "party.session", "$prototype": {"$ref": "base"},
         "players": {"$ops": [
             {"op": "update", "id": "alice", "changes": {"name": "Alice", "mode": "competitive"}},
             {"op": "insert", "value": player("carol"), "before": "bob"},
             {"op": "move", "id": "alice", "after": "bob"},
             {"op": "remove", "id": "bob"}]},
         "settings": {"$ops": [{"op": "put", "key": "scores", "value": [None, 9]}]},
         "tags": {"$ops": [{"op": "remove", "key": "pub"}, {"op": "add", "key": "arcade"}]}}],
        "root": {"$ref": "game"}}


def external(uri):
    if uri != "assets://people/players.json?revision=1":
        raise SchemaError("Unexpected external URI")
    return {"schemas": {"party.player": 1, "party.mode": 1}, "objects": [player("guest", **{"$id": "guest"})], "root": {"$ref": "guest"}}


class DataTests(unittest.TestCase):
    def setUp(self):
        self.schemas, self.maps = read(FIXTURE)
        self.catalog = Catalog(self.schemas)

    def load(self, data=None, catalog=None):
        return Loader(catalog or self.catalog, external).load(document() if data is None else data)

    def test_aggregate_tree_bytes_are_bounded(self):
        # Each string/item is legal; their aggregate must fail before copying the tree.
        with self.assertRaises(SchemaError):
            bounded_tree(["x" * 4096] * 256)
        self.assertEqual(bounded_tree(["x" * 4096] * 255), ["x" * 4096] * 255)

    def test_typed_containers_references_and_inheritance(self):
        source = document()
        original = copy.deepcopy(source)
        graph = self.load(source)
        game = graph.objects[graph.root.document, "game"]
        self.assertEqual([p["id"] for p in game["players"]], ["carol", "alice"])
        self.assertEqual(game["players"][1]["mode"], 7)
        self.assertEqual(game["settings"], [["scores", [None, 9]]])
        self.assertEqual(game["tags"], ["arcade", "party"])
        self.assertEqual(game["host"], Reference(graph.root.document, "host", "party.player"))
        self.assertEqual(game["guest"].document, "assets://people/players.json?revision=1")
        host = graph.objects[graph.root.document, "host"]
        self.assertEqual(host["friend"], game["host"])
        self.assertEqual(source, original)
        graph.objects[graph.root.document, "base"]["players"][0]["scores"][0][0] = 999
        self.assertEqual(game["players"][1]["scores"][0][0], 10)
        self.assertEqual(graph.authored[graph.root.document], original)

    def test_missing_null_reset_and_replace(self):
        data = document()
        data["objects"].append({"$id": "child", "$type": "party.player", "$prototype": {"$ref": "host"},
                                "name": {"$reset": True}, "friend": None, "scores": {"$replace": []}})
        graph = self.load(data)
        child = graph.objects[graph.root.document, "child"]
        self.assertEqual(child["name"], "Guest")
        self.assertIsNone(child["friend"])
        self.assertEqual(child["scores"], [])
        for replacement in (None, {"$reset": False}, {"$reset": True}):
            bad = copy.deepcopy(data)
            bad["objects"][-1]["id"] = replacement
            with self.subTest(replacement=replacement), self.assertRaises(SchemaError):
                self.load(bad)

    def test_conflicting_sequence_operations(self):
        operations = [
            [{"op": "remove", "id": "missing"}],
            [{"op": "insert", "value": player("alice")}],
            [{"op": "insert", "value": player("carol"), "after": "missing"}],
            [{"op": "move", "id": "alice", "before": "alice"}],
            [{"op": "move", "id": "alice", "before": "bob", "after": "bob"}],
            [{"op": "update", "id": "alice", "changes": {"id": "new"}}],
            [{"op": "update", "id": "alice", "changes": {"$prototype": {"$ref": "host"}}}],
            [{"op": "remove", "id": "alice"}, {"op": "update", "id": "alice", "changes": {}}],
        ]
        for ops in operations:
            data = document()
            data["objects"][-1]["players"] = {"$ops": ops}
            before = copy.deepcopy(data)
            with self.subTest(ops=ops), self.assertRaises(SchemaError):
                self.load(data)
            self.assertEqual(data, before)

    def test_container_duplicates_types_and_bounds(self):
        cases = [("players", [player("same"), player("same")]),
                 ("players", [player("a", scores=[[2**32]])]),
                 ("players", [player("a", mode=True)]),
                 ("settings", [["a", []], ["a", [1]]]),
                 ("settings", [["a", [True]]]), ("settings", {"a": [1]}),
                 ("tags", ["a", "a"]), ("tags", [None]), ("players", [None]),
                 ("settings", [["a", [-2**31-1]]])]
        for field, value in cases:
            data = document()
            data["objects"][1][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(SchemaError):
                self.load(data)

    def test_reference_form_target_and_forward_validation(self):
        data = document()
        data["objects"].reverse()
        self.load(data)
        for target in ({"$ref": "missing"}, {"$ref": "base"}, {"$ref": "host", "$uri": "x#y"},
                       player("inline"), {"$uri": "assets://people/players.json?revision=1#guest"}):
            data = document()
            data["objects"][1]["host"] = target
            with self.subTest(target=target), self.assertRaises(SchemaError):
                self.load(data)
        data = document()
        data["objects"].append(copy.deepcopy(data["objects"][0]))
        with self.assertRaises(SchemaError):
            self.load(data)

    def test_prototype_opt_in_type_and_cycles(self):
        data = document()
        data["objects"][1]["$prototype"] = {"$ref": "game"}
        with self.assertRaises(SchemaError):
            self.load(data)
        data = document()
        data["objects"][-1]["$prototype"] = {"$ref": "host"}
        with self.assertRaises(SchemaError):
            self.load(data)
        schemas = copy.deepcopy(self.schemas)
        next(s for s in schemas if s["id"] == "party.session")["prototype"] = False
        with self.assertRaises(SchemaError):
            self.load(catalog=Catalog(schemas))

    def test_map_set_remove_conflicts_and_patch_opt_in(self):
        for field, ops in [("settings", [{"op": "remove", "key": "missing"}]),
                           ("settings", [{"op": "put", "key": 1, "value": []}]),
                           ("tags", [{"op": "add", "key": "pub"}]),
                           ("tags", [{"op": "remove", "key": "missing"}])]:
            data = document()
            data["objects"][-1][field] = {"$ops": ops}
            with self.subTest(field=field, ops=ops), self.assertRaises(SchemaError):
                self.load(data)
        schemas = copy.deepcopy(self.schemas)
        session = next(s for s in schemas if s["id"] == "party.session")
        session["fields"][0].update(merge="replace", key="")
        with self.assertRaises(SchemaError):
            self.load(catalog=Catalog(schemas))

    def test_schema_bad_policies_and_types(self):
        for mutation in (lambda f: f.update(merge="map"), lambda f: f.update(key="missing"),
                         lambda f: f.update(storage="local"),
                         lambda f: f["type"]["arguments"][0].update(id="missing")):
            schemas = copy.deepcopy(self.schemas)
            mutation(next(s for s in schemas if s["id"] == "party.session")["fields"][0])
            with self.assertRaises(SchemaError):
                Catalog(schemas)

    def test_document_schema_versions_and_dependencies(self):
        for versions in ({"party.player": 2, "party.mode": 1, "party.session": 1},
                         {"party.player": 1, "party.session": 1}, {"unknown": 1},
                         {"party.player": True, "party.mode": 1, "party.session": 1}):
            data = document()
            data["schemas"] = versions
            with self.subTest(versions=versions), self.assertRaises(SchemaError):
                self.load(data)

    def test_record_merge_and_map_record_update(self):
        schemas = copy.deepcopy(self.schemas)
        session = next(s for s in schemas if s["id"] == "party.session")
        def field(identity, name, kind, merge):
            return {"id": identity, "name": name, "type": kind, "storage": "inline",
                    "merge": merge, "key": "", "has_default": False, "default": None}
        player_type = {"kind": "record", "nullable": False, "id": "party.player"}
        session["fields"] += [field(6, "details", player_type, "record"),
                              field(7, "captains", {"kind": "map", "nullable": False, "arguments": [
                                  {"kind": "string", "nullable": False}, player_type]}, "map")]
        data = document()
        data["objects"][1].update(details=player("details"), captains=[["red", player("captain")]])
        data["objects"][2].update(details={"name": "Overridden"}, captains={"$ops": [
            {"op": "update", "key": "red", "changes": {"name": "Captain"}}]})
        graph = self.load(data, Catalog(schemas))
        game = graph.objects[graph.root.document, "game"]
        self.assertEqual(game["details"]["id"], "details")
        self.assertEqual(game["details"]["name"], "Overridden")
        self.assertEqual(game["captains"][0][1]["name"], "Captain")
        for changes in ({"$id": "fork"}, {"$type": "party.session"}, {"$prototype": {"$ref": "host"}}):
            bad = copy.deepcopy(data)
            bad["objects"][2]["captains"]["$ops"][0]["changes"] = changes
            with self.assertRaises(SchemaError):
                self.load(bad, Catalog(schemas))

    def test_cross_document_cycles_and_prototype_scope(self):
        def linked(uri):
            data = external(uri)
            data["objects"][0]["friend"] = {"$uri": "memory://main/party.json#host"}
            return data
        graph = Loader(self.catalog, linked).load(document())
        guest = graph.objects['assets://people/players.json?revision=1', 'guest']
        self.assertEqual(guest['friend'].identity, 'host')
        def prototype(uri):
            data = external(uri)
            data['objects'][0] = {'$id': 'guest', '$type': 'party.player', 'id': 'guest',
                                  '$prototype': {'$uri': 'memory://main/party.json#host'}}
            return data
        graph = Loader(self.catalog, prototype).load(document())
        guest = graph.objects['assets://people/players.json?revision=1', 'guest']
        self.assertEqual(guest['friend'].document, graph.root.document)
        data = document()
        data['objects'][0]['$prototype'] = {'$uri': 'assets://people/players.json?revision=1#guest'}
        with self.assertRaises(SchemaError):
            Loader(self.catalog, prototype).load(data)

    def test_named_inline_identity_cannot_be_forked_by_merge(self):
        data = document()
        data['objects'][1]['players'][0]['$id'] = 'named_alice'
        with self.assertRaises(SchemaError):
            self.load(data)
        data['objects'][2].pop('players')
        graph = self.load(data)
        self.assertIn((graph.root.document, 'named_alice'), graph.objects)
        self.assertEqual(graph.objects[graph.root.document, 'game']['players'][0]['name'], 'Guest')

    def test_malformed_control_data_reports_validation_errors(self):
        changes = [lambda d: d['objects'][0].update({'$type': []}),
                   lambda d: d['objects'].append(123),
                   lambda d: d['objects'][2].update(tags={'$ops': [{'op': [], 'key': 'pub'}]}),
                   lambda d: d['objects'][1].update(guest={'$uri': 'asset://[bad#x'}),
                   lambda d: d['objects'][1].update(guest={'$uri': 'asset://x#%ff'})]
        for mutation in changes:
            data = document()
            mutation(data)
            with self.assertRaises(SchemaError):
                self.load(data)
        def bad_root(uri):
            data = external(uri)
            data['root'] = {'$ref': 'missing'}
            return data
        with self.assertRaises(SchemaError):
            Loader(self.catalog, bad_root).load(document())

    def test_passive_source_rejects_executable_constructs(self):
        for source in ("import os", "CONTAINER_MAPS = side_effect()", "x = 1",
                       "@schema('a', version=1, name='a')\nclass A:\n def f(self): pass"):
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "bad.py"
                path.write_text(source)
                with self.subTest(source=source), self.assertRaises(SchemaError):
                    read(path)

    def test_budget_and_single_transaction(self):
        loader = Loader(self.catalog, external)
        loader.load(document())
        with self.assertRaises(SchemaError):
            loader.load(document())
        data = document()
        data["objects"][0]["scores"] = [[1] * 4097]
        with self.assertRaises(SchemaError):
            self.load(data)
        nested = []
        for _ in range(60):
            nested = [nested]
        with self.assertRaises(SchemaError):
            self.load({"objects": nested, "root": {"$ref": "x"}})


if __name__ == "__main__":
    unittest.main()
