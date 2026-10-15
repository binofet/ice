"""Independent Python authoring fixture for the rich portable-data consumer."""
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Annotated
from metagen import schema, symbol, i32, u32

# Module scope is Python's equivalent of these assembly-scoped mapping declarations.
# The reader inspects literals; it never imports metagen or executes this module.
CONTAINER_MAPS = [
    {"source": "list", "shape": "sequence", "target": "cpp20", "include": "vector",
     "type_template": "std::vector<{0}>", "adapter": "metagen_examples::vector_adapter"},
    {"source": "list", "shape": "sequence", "target": "cpp23", "include": "playground/custom_vector.h",
     "type_template": "playground::custom_vector<{0}>", "adapter": "playground::vector_adapter"},
    {"source": "dict", "shape": "map", "target": "cpp20", "include": "map",
     "type_template": "std::map<{0}, {1}>", "adapter": "metagen_examples::map_adapter"},
    {"source": "list", "shape": "sequence", "target": "rust2021", "include": "std/vec",
     "type_template": "Vec<{0}>", "adapter": "metagen::sequence_adapter"},
]

@schema("party.mode", version=1, name="mode")
class Mode(IntEnum):
    casual = symbol(1, 0)
    competitive = symbol(2, 7)

@schema("party.player", version=1, name="player", prototype=True)
@dataclass
class Player:
    id: str = field(metadata={"id": 1})
    name: str = field(default="Guest", metadata={"id": 2})
    mode: Mode = field(default=0, metadata={"id": 3})
    friend: Player | None = field(default=None, metadata={"id": 4, "storage": "any"})
    scores: list[list[Annotated[int, u32]]] = field(metadata={"id": 5})

@schema("party.session", version=1, name="session", prototype=True)
@dataclass
class Session:
    players: list[Player] = field(metadata={"id": 1, "merge": "keyed", "key": "id"})
    settings: dict[str, list[Annotated[int, i32] | None]] = field(metadata={"id": 2, "merge": "map"})
    tags: set[str] = field(metadata={"id": 3, "merge": "set"})
    host: Player = field(metadata={"id": 4, "storage": "local"})
    guest: Player = field(metadata={"id": 5, "storage": "external"})
