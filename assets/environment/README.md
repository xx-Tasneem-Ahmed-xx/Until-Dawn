## Environment Assets (Member 4)

This folder contains all world-building assets used by Member 4 tasks.

### Structure

- `modular/` - Buildings, walls, floors, stairs, gates, and reusable level pieces.
- `props/` - Crates, barrels, lamps, carts, fences, debris, and other scene props.
- `textures/` - Ground, wall, roof, and decal textures (plus normal/roughness maps if available).
- `materials/` - Material configs and notes used by the renderer setup.
- `sources/` - Original downloaded archives or source files (kept for traceability).

### Import Rules

1. Prefer `.obj` for models to match the current loader support.
2. Use lowercase snake_case names, for example: `env_house_a.obj`, `prop_barrel_01.obj`.
3. Keep scene scale consistent (1 world unit = 1 meter recommended).
4. Save license and source links in `ASSETS_CREDITS.md` at repo root.
5. Add colliders in JSON scenes for static gameplay blockers.

### Suggested First Pack

- 4-6 building modules
- 6-10 props
- 2-4 ground textures
- 1-2 decorative decal textures

### Auto Integration

1. Place OBJ files using names in `assets/environment/ASSET_MANIFEST.json`.
2. Run:
   - `python3 scripts/generate_town.py`
3. The script auto-detects available online meshes and inserts them into `config/game-phase2.jsonc`.
4. Missing meshes gracefully fall back to `cube`, so generation never breaks.
