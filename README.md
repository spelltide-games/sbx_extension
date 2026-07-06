# sbx_extension

`sbx_extension` is a native extension package for [godot-pocketpy](https://github.com/spelltide-games/godot-pocketpy).
It provides runtime modules used by SBX gameplay code in both Godot and PocketPy.

## What this repo contains

- Core extension glue in `include/` and `src/`.
- Third-party networking transport (`kcp/`, from ikcp).
- Embedded LevelDB source (`leveldb/`) compiled as part of this project.
- Python typing stubs in `typings/sbxcpp/` for PocketPy modules.

## Components and features

### 1) LevelDB

Module: `sbxcpp.leveldb`

Feature highlights:
- Persistent key-value storage backed by embedded LevelDB.
- Full CRUD operations: `get`, `put`, `delete`.
- Atomic batch writes with `write(ops)` where values can be set or removed.
- Ordered key iteration with optional `start` and `end` bounds.
- Optional Bloom filter configuration via `bloom_filter_policy_bits` when opening a DB.
- Path normalization through Godot `ProjectSettings.globalize_path`, so project-relative paths are supported.

Typical use:
- Save world state, player snapshots, lockstep metadata, and cached generated data.

### 2) Cube physics

Module: `sbxcpp.space`

Feature highlights:
- A lightweight 3D physics layer built around cube-like bodies (`Cube` with optional rounded radius).
- Supports `DYNAMIC`, `KINEMATIC`, `STATIC`, and `TILE` body kinds.
- Broad-phase collision acceleration via chunked spatial partitioning.
- Narrow-phase contact solving using axis-aligned faces and penetration separation.
- Trigger-capable bodies and collision event callbacks (`ADDED` / `REMOVED`).
- Per-body runtime control for layer, mass, velocity, instant velocity, position, extent, and size.

Typical use:
- Deterministic-ish gameplay movement and collisions for tile worlds and moving entities.

### 3) Torus tilemap

Module: `sbxcpp.space` (`Tilemap`, torus helpers)

Feature highlights:
- 2D tilemap with layered tiles (`base`, `floor`, `slime`, `block`).
- Wrap-around (torus) addressing for both tile indexing and collision/chunk queries.
- Supports slicing views of a shared tilemap without copying tile storage.
- Utilities to normalize AABBs and iterate chunk ranges across wrap boundaries.
- Tile bodies can participate in the same collision pipeline as moving bodies.

Typical use:
- Seamless looping maps where entities crossing one edge reappear on the opposite edge.

### 4) Multiplayer

Module: `sbxcpp.lockstep`

Feature highlights:
- WebSocket channel for session/control traffic.
- UDP + KCP channel for low-latency, reliable game data.
- Configurable UDP redundancy for packet re-send fanout.
- Explicit polling model (`poll()`) that dispatches `on_ws_data` and `on_kcp_data` callbacks.
- Connection status flags for lifecycle checks (`ws_opened`, `ws_closed`, `kcp_opened`).

Typical use:
- Lockstep networking where control-plane messages use WebSocket and frame/input transport uses KCP.

## Serialization helper

Godot class: `MessagePack`

Feature highlights:
- `MessagePack.loads(bytes)` and `MessagePack.dumps(Variant)` bridge Godot `Variant` values with MessagePack binary payloads.
- Supports nil, bool, int/uint, float/double, strings, byte arrays, arrays, and dictionaries.

This is commonly used together with the multiplayer module and LevelDB persistence.

## Build notes

- Built with CMake (`C++17`).
- Compiles this extension as a static library.
- Builds bundled `leveldb/` and links with `lz4`, `godot-cpp`, and `pocketpy`.


