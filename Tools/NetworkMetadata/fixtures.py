"""Passive declarations: importing or executing this file is never required."""
from dataclasses import dataclass, field
from typing import Annotated
from metagen import schema, f32, u32


@schema("demo.transform", version=1, name="transform", tags={
    "ice.net.writer": "authority", "ice.net.audience": "relevant",
    "ice.net.delivery": "snapshot", "ice.net.conflict": "whole_field_cas"})
@dataclass
class Transform:
    x: Annotated[float, f32] = field(metadata={"id": 1, "range": [-300, 300], "tags": {
        "ice.net.codec": "signed16_cm", "ice.net.codec_version": "1", "ice.net.unit": "m", "ice.net.interpolation": "linear"}})
    y: Annotated[float, f32] = field(metadata={"id": 2, "range": [-300, 300], "tags": {
        "ice.net.codec": "signed16_cm", "ice.net.codec_version": "1", "ice.net.unit": "m", "ice.net.interpolation": "linear"}})
    z: Annotated[float, f32] = field(metadata={"id": 3, "range": [-300, 300], "tags": {
        "ice.net.codec": "signed16_cm", "ice.net.codec_version": "1", "ice.net.unit": "m", "ice.net.interpolation": "linear"}})


@schema("demo.health", version=1, name="health", tags={
    "ice.net.writer": "authority", "ice.net.audience": "relevant",
    "ice.net.delivery": "reliable_change", "ice.net.conflict": "whole_field_cas"})
@dataclass
class Health:
    points: Annotated[int, u32] = field(metadata={"id": 1, "range": [0, 65535], "tags": {
        "ice.net.codec": "uint16_exact", "ice.net.codec_version": "1", "ice.net.unit": "points", "ice.net.interpolation": "none"}})


@schema("demo.fire_event", version=1, name="fire_event", tags={
    "ice.net.direction": "client_to_authority", "ice.net.capability": "weapon.fire",
    "ice.net.delivery": "reliable_event", "ice.net.deduplication": "sequence_window_epoch"})
@dataclass
class FireEvent:
    weapon: str = field(metadata={"id": 1, "tags": {"ice.net.codec": "entity_v1", "ice.net.codec_version": "1",
        "ice.net.unit": "entity", "ice.net.interpolation": "none", "ice.net.reference": "owned_weapon"}})
    input_sequence: Annotated[int, u32] = field(metadata={"id": 2, "range": [0, 4294967295], "tags": {
        "ice.net.codec": "uint32_exact", "ice.net.codec_version": "1", "ice.net.unit": "sequence", "ice.net.interpolation": "none"}})
    aim_x: Annotated[float, f32] = field(metadata={"id": 3, "range": [-1, 1], "tags": {
        "ice.net.codec": "float32_finite", "ice.net.codec_version": "1", "ice.net.unit": "unit_vector", "ice.net.interpolation": "none"}})
    aim_y: Annotated[float, f32] = field(metadata={"id": 4, "range": [-1, 1], "tags": {
        "ice.net.codec": "float32_finite", "ice.net.codec_version": "1", "ice.net.unit": "unit_vector", "ice.net.interpolation": "none"}})
    aim_z: Annotated[float, f32] = field(metadata={"id": 5, "range": [-1, 1], "tags": {
        "ice.net.codec": "float32_finite", "ice.net.codec_version": "1", "ice.net.unit": "unit_vector", "ice.net.interpolation": "none"}})
