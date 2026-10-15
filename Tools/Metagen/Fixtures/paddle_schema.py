# Declarative schema source. Metagen reads this file's AST; it never imports it.
from dataclasses import dataclass, field
from typing import Annotated
from metagen import schema, f32, i32, u32


@schema("playground.paddle_settings", version=2, name="paddle_settings",
        tags={"game.category": "gameplay"})
@dataclass
class PaddleSettings:
    maximum_speed: Annotated[float, f32] = field(default=2.8, metadata={"id": 1, "range": [0, 20]})
    predict_rebounds: bool = field(default=True, metadata={"id": 2})
    winning_score: Annotated[int, u32] = field(default=500, metadata={"id": 3, "range": [1, 10000]})
    player_name: str = field(default="Player", metadata={"id": 4, "tags": {"editor.label": "Player name"}})
    boost: Annotated[float, f32] | None = field(default=None, metadata={"id": 5, "range": [0, 10]})
    simulation_seed: Annotated[int, i32] = field(default=-7, metadata={"id": 6, "range": [-1000, 1000]})
