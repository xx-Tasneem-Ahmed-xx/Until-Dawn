# Member 4 Online Environment Tasks

This checklist is a complete, practical plan for integrating online environment objects into the current project.

## Goal

Build a playable, readable arena using downloaded environment assets while keeping collision and licensing clean.

## Phase 1 - Download and Organize

- [ ] Download one modular environment pack (buildings + walls + stairs).
- [ ] Download one props pack (crates, barrels, lamps, fences).
- [ ] Download 2-4 tileable textures (cobblestone, dirt, roof, wall).
- [ ] Place files in:
  - `assets/environment/modular/`
  - `assets/environment/props/`
  - `assets/environment/textures/`
  - `assets/environment/sources/`
- [ ] Register every pack in `ASSETS_CREDITS.md`.

## Phase 2 - Convert and Normalize

- [ ] Ensure models are in `.obj` format.
- [ ] Normalize naming to lowercase snake_case.
- [ ] Keep pivots and scales consistent so scene placement is easy.
- [ ] Remove unused high-poly variants if low-poly alternatives exist.

## Phase 3 - Scene Assembly

- [ ] Open `config/game-phase2.jsonc`.
- [ ] Replace repeated primitive blocks with environment meshes where possible.
- [ ] Keep colliders for all blocking geometry (walls, large props, boundaries).
- [ ] Keep gameplay lanes clear for player movement and zombie navigation.

## Phase 4 - Visual Quality Pass

- [ ] Add key lights near paths and combat areas.
- [ ] Ensure silhouette readability at night (enemies must stay visible).
- [ ] Add prop variation so repeated modules do not look cloned.
- [ ] Test from gameplay camera angles, not only free camera.

## Phase 5 - Team Integration

- [ ] Share collider boundaries with Member 1 (player movement).
- [ ] Share obstacle map with Member 3 (zombie pathing behavior expectations).
- [ ] Confirm firing lanes with Member 2 (combat readability and cover).

## Recommended Sources

- Kenney (CC0): https://kenney.nl/assets
- Poly Pizza: https://poly.pizza/
- OpenGameArt: https://opengameart.org/
- Sketchfab (filter by downloadable + allowed license): https://sketchfab.com/

## Done Criteria

Member 4 task is complete when:

1. Arena uses real environment assets rather than only primitive placeholders.
2. All blockers have valid colliders.
3. Scene remains playable for player and zombies.
4. Every external asset is documented in `ASSETS_CREDITS.md`.
