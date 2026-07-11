# DMO Battle-Hex Heightmap Package

**Source**: `UE Plugin Pieces/ChunkLevelGenerator/Content/StampIt/Examples/`  
**Imported**: 2026-04-13  
**Used by**: `DMOBattleHexChunkEditorWindow` (WICKED-MAP-08A)

## Contents

52 biome-specific heightmap PNGs from the StampIT collection. These are loaded as `wi::Resource` textures in the battle-hex chunk editor window to provide topographic variation for the `TopographyRiseVector` and `OrientationBucket` controls.

## Biome Coverage

| Heightmap family | Battle-hex biome |
|---|---|
| `HM_Canyon_*` (3 files) | Canyons |
| `HM_Dunes_*`, `HM_Dune_Shapes_*` (4 files) | Desert |
| `HM_Highlands_*` (2 files) | Hills |
| `HM_Meadows_*` (3 files) | Plains / Settled/Farmland |
| `HM_Rocky_Desert_*`, `HM_Rugged_Rocks_*`, `HM_Rugged_Sedi_Rocks_*` (6 files) | Desert / Mountains |
| `HM_Rolling_Hills_*` (1 file) | Hills |
| `HM_Tundra_*` (2 files) | Northern Forest |
| `HM_Volcano_*` (3 files) | Mountains |
| `HM_Sandy_Beaches_*`, `HM_Seaside_Cliffs_*` (3 files) | Riverbank / Ocean |
| `HM_Valleys_*` (1 file) | Plains |
| `HM_Rough_Cliffs_*`, `HM_Terraced_Cliffs_*` (5 files) | Mountains / Canyons |
| `HM_Islands_*` (2 files) | Lake / Ocean |
| `HM_Stranger_Lands_*` (3 files) | Jungle / Tropical Hilly |
| `HM_Pride_Rocks_*` (3 files) | Mountains |
| `HM_Impact_*` (2 files) | Canyons |
| `HM_Dover_Cliffs_*` (2 files) | Mountains |
| `HM_Rocky_Plateaus_*` (1 file) | Mountains |
| `HM_Alien_World_*` (3 files) | Exotic / special encounters |
| `HM_Monument_Desert_*` (1 file) | Desert |
| `HM_Craters_*` (1 file) | Special encounters |
| `HM_Terrace_Fields_*` (1 file) | Settled/Farmland |

## Usage in DMOBattleHexChunkEditorWindow

```cpp
// Enumerate heightmaps for the ComboBox selector
std::vector<std::string> heightmapPaths;
wi::helper::GetFileNamesInDirectory(
    "Content/terrain/dmo-battle-hex-heightmaps", heightmapPaths, "png");

// Load selected heightmap as wi::Resource
wi::Resource heightmapResource = wi::resourcemanager::Load(selectedPath);
```

The heightmap is used to:
1. Drive the terrain preview canvas in the editor viewport
2. Derive the `topographyRiseVector` from the heightmap gradient at the chunk center
3. Compute the `orientationBucket` from the dominant slope direction

## License

StampIT heightmaps are from the "Ultimate StampIT Collection for UE" asset pack. See `UE Plugin Pieces/ChunkLevelGenerator/Ultimate StampIT Collection for UE.pdf` for license terms. These are used as editor-only authoring assets and are not shipped in production client builds.
