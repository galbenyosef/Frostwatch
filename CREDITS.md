# Frostwatch — The Last Ember: credits and third-party notices

The current public preview is an Unreal Engine game. The contributor records below preserve both current credits and the earlier Godot edition's history. Godot engine notices, Piper narration and superseded character assets describe that earlier edition; they do not imply those components ship in the current Unreal build. Current dialogue, including inn tales, is rendered with Kokoro. The v0.4 packaged asset inventory excludes the old Piper tale recordings.

Thank you to all artists, recordists, animators, tool authors and engine contributors listed here. Their work remains theirs; inclusion does not imply endorsement. See [DISTRIBUTION.md](DISTRIBUTION.md) for how the packaged game and its licensed components may be shared.

## Historical native edition and retained asset credits

Game code, world layout, quests, adapted animations, shaders, interface, and the remaining synthesized ambience/reward cues were created for this project. Human models and footsteps use the assets credited below. The native edition uses Godot 4.7.2 and its Vulkan Forward+ renderer.

## Human knight model (CC0)

**crownjoshua**, *Knight (Rigged - Mid Poly)*: https://opengameart.org/content/knight-rigged-mid-poly

The source knight, armour, facial features and associated material images are distributed by the author under CC0. For this game, the model was converted from its original Blender file into a local GLB, reduced to a 19-bone game skeleton, given new idle/walk/attack/death animation clips and PBR materials, and adapted into guards, Captain Rowan, Edrin, Ste Chlomaine and Stu Geramos. The original source file and unrelated reference images are not bundled in the playable build. Hair, crown and weapons are game additions. All required character assets are included locally; no account or network connection is required.

## Recorded footsteps (CC0)

- **Corsica_S**, *42 Snow and Gravel Footsteps*, extracted and published by **Iwan Gabovitch / qubodup** with the recordist's permission under CC0: https://opengameart.org/content/42-snow-and-gravel-footsteps
- The 18 snow-covered-gravel recordings form the snow bank; the 24 gravel-and-ice recordings form the icy-road bank. Source recordings: https://freesound.org/people/Corsica_S/sounds/28661/ and https://freesound.org/people/Corsica_S/sounds/28662/
- **Kenney**, *Impact Sounds 1.0*, five concrete footstep recordings used for the stone hall, CC0: https://kenney.nl/assets/impact-sounds

The recordings retain their original sample rate and are stored as 16-bit PCM WAV. Preparation downmixed the source to mono for positional playback, gently removed low-frequency handling rumble, trimmed silence, faded the boundaries, and matched levels without clipping. No tonal footstep synthesizer is used. Runtime playback adds non-repeating variation, restrained pitch and level changes, alternating foot positions, and a subtle indoor reverb.

## Poly Haven assets (CC0)

These assets are bundled locally; the game needs no network connection.

- Modular Fort 01: https://polyhaven.com/a/modular_fort_01
- Kite Shield, by Ulan Cabanilla: https://polyhaven.com/a/kite_shield
- Castle Brick 02 White: https://polyhaven.com/a/castle_brick_02_white
- Snow 02: https://polyhaven.com/a/snow_02
- Rock Boulder Dry: https://polyhaven.com/a/rock_boulder_dry
- Belfast Sunset (Pure Sky): https://polyhaven.com/a/belfast_sunset_puresky

Models were arranged, scaled, and given snow overlays in the game. Materials use locally bundled 2K texture maps with GPU compression, mipmaps, and anisotropic filtering. Powered by Poly Haven: https://polyhaven.com/

## Generated foliage texture

`Source/assets/spruce-branch-snow.png` was generated with the built-in image generation tool. It is an actual RGBA image, 1254 × 1254 pixels, used on animated, crossed foliage surfaces. The generation brief was a photographic Norway spruce branch, laid flat with its central stem vertical, asymmetric side twigs, individual needles, about 20% fresh snow, neutral lighting, and an actual transparent background. The runtime adds shape, instancing, movement, and lighting; the texture is not a photograph of a real location.

## Engine and system fonts

Godot is licensed under MIT. Its full license is in `GODOT-LICENSE.txt`; bundled engine component notices are in `THIRD-PARTY-LICENSES.txt`. See https://godotengine.org/license/

The interface uses available system fonts (Georgia / Segoe UI on Windows, with DejaVu and generic fallbacks on Linux). Proprietary font files are not bundled.

## Creative inspiration

Frostwatch is an independent prototype inspired by northern medieval fantasy and stealth adventures. No Game of Thrones or Assassin's Creed game assets, logos, character models, or recordings are included. Stu Geramos and Ste Chlomaine are the user-requested boss names.


## Living World & Combat additions

- **Drummyfish and Cethiel — Cethiel's Dragon 3D**, CC0: https://opengameart.org/content/cethiels-dragon-3d . Converted from the author's Blender source to GLB; retained and adapted idle/walk/attack/death animation, added a flight loop, recoloured and scaled for the two drakes. This is a low-poly painted model, not a scanned photorealistic dragon.
- **RandomMind — Medieval: The Old Tower Inn** and **Medieval: The Bard's Tale**, CC0: https://opengameart.org/content/medieval-the-old-tower-inn and https://opengameart.org/content/medieval-the-bards-tale . Original looping WAVs are included, with runtime spatial playback and dialogue ducking.
- **Michel Baradari — 11 male human pain/death sounds**, submitted by qubodup, CC BY 3.0: https://opengameart.org/content/11-male-human-paindeath-sounds . License: https://creativecommons.org/licenses/by/3.0/ . Original human recordings are renamed into the voice bank, with runtime gain/pitch variation and positional playback. Author site: https://apollo-music.de/ . No endorsement is implied.
- **remaxim**, based on **qubodup** — 3 Melee sounds, CC0: https://opengameart.org/content/3-melee-sounds . Sword and melee swishes are used with runtime level/pitch changes.
- **Independent.nu**, published by **Iwan Gabovitch / qubodup** — 8 wet squish, slurp impacts, CC0: https://opengameart.org/content/8-wet-squish-slurp-impacts . Converted from FLAC to mono 16-bit PCM WAV for game impacts. Original source: https://web.archive.org/web/20120227033703/http://www.independent.nu/ .
- **Kenney — Impact Sounds**, CC0: https://kenney.nl/assets/impact-sounds . Five wooden footsteps and five medium metal impacts extend the previously credited stone-footstep bank.
- **Poly Haven — Wood Planks Dirt**, CC0: https://polyhaven.com/a/wood_planks_dirt . 2K diffuse, OpenGL normal and roughness maps are used for inn floors, furnishings and timber structures, with runtime tinting and triplanar mapping.

The crownjoshua human model described above is now divided at anatomical planes while retaining skin weights, UVs and material maps. Cut surfaces were added. The derived rig is also used for the original villagers and fantasy orcs; tusks and ears are new geometry. Runtime severing bakes only the separated piece and preserves its materials. Wounds and blood patterns are generated by game code.

### Offline story narration

The nine tale recordings in `Source/assets/living-audio/tales` were synthesized locally with **Piper**, using **Michael Hansen / Rhasspy's en_GB northern_english_male medium** model. They are generated narration, not a new human voice performance or an imitation requested of a named person. Piper tooling and the neural model are not included in the playable distribution; only the resulting WAVs and transcripts are bundled.

Model card: https://huggingface.co/rhasspy/piper-voices/blob/main/en/en_GB/northern_english_male/medium/MODEL_CARD . Piper: https://github.com/rhasspy/piper . The voice derives from the **OpenSLR SLR83 UK and Ireland English dialect corpus**, copyright 2018, 2019 Google, Inc., by **Isin Demirsahin, Oddur Kjartansson, Alexander Gutkin and Clara Rivera**: https://www.openslr.org/83/ . Its model card specifies **CC BY-SA 4.0**. The bundled generated tale WAVs and their transcripts are provided under the same CC BY-SA 4.0 license: https://creativecommons.org/licenses/by-sa/4.0/ . Changes comprise synthesis of the original Frostwatch tales, timing and conversion into game-ready clips. This license statement applies to these tale recordings and transcripts; independently licensed assets retain their own licenses. No endorsement is implied.


## Riding and rendering update

- **Lyndon Daniels**, Rigged Horse (rigging contributed by **ChadM**), **CC0**: https://opengameart.org/content/rigged-horse . Packed 2K coat/hair textures and eye texture are retained; legacy materials were rebuilt as PBR materials. Unweighted eyes, mane and tail received bone weights. Idle, walk, trot and gallop clips, normalized scale, saddle/harness, carriage, reins and gameplay were prepared for Frostwatch. Original mesh/rig downloaded as `riggedHorse.blend`.
- **congusbongus**, Horse gallop on different surfaces: https://opengameart.org/content/horse-gallop-on-different-surfaces . Four supplied recordings are used: `grass` from StephenSaldanha (CC0, https://freesound.org/people/StephenSaldanha/sounds/165532/), `ground` from D4XX (CC0, https://freesound.org/people/D4XX/sounds/564628/), `muffled` from MrFizzywig (CC0, https://freesound.org/people/MrFizzywig/sounds/581834/), and `hall` from Twigy233 (CC BY 4.0, https://freesound.org/people/Twigy233/sounds/683471/). The supplied edited recordings are unchanged; playback gain, cadence and pitch vary in game. Credits are also preserved in `Source/assets/riding/hoof-credits.txt`. CC BY 4.0: https://creativecommons.org/licenses/by/4.0/ . CC0: https://creativecommons.org/publicdomain/zero/1.0/ . The included ground/muffled variations currently provide riding playback; grass and hall recordings are available in the riding audio bank.
- **Poly Haven**, Cobblestone Floor 08, **CC0**: https://polyhaven.com/a/cobblestone_floor_08 . 2K diffuse, OpenGL normal, roughness and ambient-occlusion maps are used on the inn forecourts. Source images are retained; engine import generates GPU-compressed mip chains. The same import optimization was applied to existing 3D textures.
- **AMD FidelityFX Super Resolution**, as integrated in Godot Engine. FSR 1 and FSR 2 are optional runtime rendering modes. Engine third-party license notices are included in `THIRD-PARTY-LICENSES.txt`.


## September 2026 makeover

Authored slate courses, construction details, undergrowth geometry and shaders were created for this project. The makeover reuses the already credited Poly Haven stone, snow and modular-fort plaster scans and existing character textures. No new third-party asset or license was introduced.

## Witnesses and Dragonfire additions · 12 September 2026

The cave topology, procedural wall/roof meshes, new cottage forms, relic quest writing, sustained fire shader, rein curves, instrument props, arm-target solver and mix configuration were authored for this prototype. They reuse the textures, human/horse/dragon models, shield and CC0 RandomMind recordings credited above. No new third-party assets were downloaded for this revision. The band animations are procedural and do not reproduce a recorded performer.

## Unreal migration additions

- Epic Games, Paragon: Greystone: https://www.fab.com/listings/122fd7bf-6f12-4304-a930-cccbbacdaebc . Acquired free through the user's Fab library and installed on 13 September 2026. Character meshes, materials, animations and physics assets are used for the Unreal gate encounter. The package retains its own Fab/Epic license; it is not CC0. Frostwatch does not use the Paragon trademark as its game name or advertising name.
- Poly Haven, Pine Sapling Small, CC0: https://polyhaven.com/a/pine_sapling_small . Original 2K glTF model and texture maps downloaded for the Unreal forest test area. Source files are retained under SourceAssets/pine_sapling_small.
- Poly Haven, Rock Moss Set 01, CC0: https://polyhaven.com/a/rock_moss_set_01 . Original 2K glTF model and texture maps downloaded for the Unreal gate approach. Source files are retained under SourceAssets/rock_moss_set_01.

The historical Godot engine notices above apply to the preserved Godot release. This migration project uses Unreal Engine 5.8.2 under Epic's Unreal Engine license; do not label an Unreal build as MIT-licensed merely because the predecessor used Godot.

## Owned orc assets imported on 13 September 2026

- Inventive World, **Orc Warrior Character 3D Model - Fantasy Creature**: https://www.fab.com/listings/5cc74fa2-7695-4f28-9bfb-b8dddb63287a . Downloaded from the user's Fab library. Original GLB retained in `SourceAssets/Orcs/InventiveWorld`; imported static mesh and textures in `/Game/Orcs/InventiveWorld`. This GLB contains no skeleton or animation. Retains its Fab license; not represented as CC0.
- Yagami, **Orc Character - Low Poly Rigged Game Ready Asset**, Fab library product folder identifier `0bc2b1d0`. Original `g0blin100.fbx` retained in `SourceAssets/Orcs/Yagami`; skeletal mesh, textures, physics and original T-pose clips imported in `/Game/Orcs/Yagami`. Retains its Fab license; not represented as CC0. Frostwatch combat clips are retargeted from the separately credited Epic Games Greystone animation library.

Boss candidates in `BOSS-ART-SHORTLIST.md` are research recommendations. Listing them is not an assertion that Sevarog, Kwang or Grux has been acquired or incorporated into the distributed game.

## Additional CC0 environment assets

Downloaded from Poly Haven for the Unreal environment pass, 13 September 2026:
- Fir sapling: https://polyhaven.com/a/fir_sapling
- Wooden crate 01: https://polyhaven.com/a/wooden_crate_01
- Barrel 03: https://polyhaven.com/a/barrel_03
- Grey roof tiles: https://polyhaven.com/a/grey_roof_tiles

Source files and download/hash records are retained in SourceAssets/Environment and Verification/environment-downloads.json.

## Owned Medieval Village scenery

Quixel Megascans, **Medieval Village Megascans Sample**, acquired through the user's Fab library on 13 September 2026. Selected scanned barrels, wheelbarrows, hitching posts, cart wheels, vessels, stone walls, rocks, fallen timber and moss are used in the Unreal campaign. These assets retain their applicable Fab/Epic license and are not CC0. Only the selected assets and their material dependencies were copied into the game; the sample's gameplay systems were not imported. Selection and copy records are in `Verification/owned-scenery-selection.json` and `Verification/owned-scenery-copy.json`. No purchase or paid service was used for this integration.

## Local dialogue rendering

## Free civilian locomotion, 14 September 2026

Quaternius and contributing animator Gonzalo Furnier, **Universal Animation Library, Standard**, CC0: https://quaternius.itch.io/universal-animation-library . The free Standard archive was downloaded using the no-payment option. The in-place walk, idle, talking idle and interaction clips were retargeted to Frostwatch's existing modular civilian skeleton. Original archive, in-place GLB, README and license are retained under `SourceAssets/Animations/Quaternius`. Paid Pro and Source editions were not acquired.

## Local speech voices

Additional subtitle-matched speech was rendered locally using installed Windows speech voices (Microsoft George and Hazel Desktop). This is synthetic speech, not recordings of human performers. Source text, voice choice and generated filenames are recorded in SourceAssets/Voices/catalog.json. No paid speech service was used.

## Ste Chlomaine: DemonLord2

DemonLord2 by Leks, confirmed owned in the user's Fab library on 14 September 2026. The Unreal 5.7 edition was added to the Unreal 5.8 project without purchase. Its armored mesh with integrated hammer and original idle, run, attack, damage and death clips are used for Ste Chlomaine (jailer). The asset retains its applicable Fab license; it is not CC0. Textures are capped at 2048 for the target laptop. See Verification/ste-demon-lord.json for integration status.


## Player sword, 14 September 2026

Sword by AGTRI Studios, acquired from the user's owned Fab library without purchase. The original MedievalSword static mesh and supplied material are used as the visible first-person sword. The asset retains its applicable Fab license and is not CC0. See Verification/owned-sword-bounds.json.

# Living scenery additions — 14 September 2026

Acquired from the user's already owned Fab library, with no purchase in this pass:

- **Procedural Nature Seasons Pack — PurePolygons:** selected winter trees, shrubs, grass, snow banks, fences, mountains, terrain textures and snowfall particles.
- **Light Foliage — MYTHRA TECH:** selected ground foliage and shrubs.
- **Water Materials — tharlevfx:** lake and waterfall materials and their dependencies. The revised Widow's Veil also uses the pack's original waterfall base and arc meshes, matching material instances, a river surface, foam and splash emitters, adapted from its example assembly at a smaller scale. Its rectangular demonstration surrounds are replaced with terrain fitting and scanned rocks.
- **Hebe Fountain — Pierogi3:** assembled fountain, animated streams, ripples and the supplied splash sound.
- **Dynamic Sky & Light Manager — Procedural World Lab:** selected sky panoramas and cloud texture. Its incompatible controller is not used; the game supplies its own clock-driven controller.

These assets remain subject to their respective Fab licence terms. Their source project/demo maps are not part of the playable campaign.

## Recorded sword contact — 15 September 2026

Ben Jaszczak and Brian Nelson, **Medieval sound effects: Weapon impacts / Weapon Textures**, CC0: https://opengameart.org/node/146863 and https://opengameart.org/content/medieval-sound-effects-weapon-textures . Four swings, six clashes and four impacts were selected from recordings, converted to mono 48 kHz PCM, trimmed, faded and levelled. These replace the previous combat tones.

## Mountain Dragon — 15 September 2026

**Quadruped Fantasy Creatures**, PROTOFACTOR INC, confirmed owned in the user's Fab library and imported without purchase. Listing: https://www.fab.com/listings/52d686b6-1180-4f26-901f-ce3c69a14767 . Selected Mountain Dragon mesh, physics, textures and original flight, breath and death animations are used for Ashwing and Rimefang, with game-specific material tinting. This asset retains its applicable Fab licence; it is not CC0. Other creatures and demonstration maps from the pack are excluded from the game integration.

## Waterfall and stream recordings — 15 September 2026

Blender Foundation, **Ambient Mountain, River, Wind and Forest and Waterfall (Yo Frankie!)**, submitted by Lamoot. Source: https://opengameart.org/content/ambient-mountain-river-wind-and-forest-and-waterfall . Licensed under CC BY 3.0: https://creativecommons.org/licenses/by/3.0/ . Selected waterfall and river recordings were downmixed to mono, crossfaded for looping and peak-normalised for positional ambience. Source and output hashes are recorded in SourceAssets/WaterfallAudio/LICENSE-SOURCES.json.

## Character dialogue — 15 September 2026

The current Unreal release replaces the older Windows speech recordings with locally generated **Kokoro v1.0** dialogue. Model by **hexgrad / rzvzn**, Apache-2.0: https://huggingface.co/hexgrad/Kokoro-82M . Offline authoring uses **thewh1teagle / kokoro-onnx**, MIT: https://github.com/thewh1teagle/kokoro-onnx . The model and speech-generation tooling are not included in the game; the distribution contains the resulting recordings only.

Seven voice profiles use the British English `bm_daniel`, `bm_fable`, `bm_george` and `bm_lewis` voices and equal-weight blends. Casting follows the masculine character models currently used for the captain, cartographer, townspeople and inn staff, with slower delivery for the woodland elders. Dialogue is synthetic; it is not a commissioned human performance or a custom imitation of a named person. Sentence chunking, delivery speed and amplitude were adjusted for Frostwatch. All game dialogue text remains the original Frostwatch writing.

Kokoro's linked model card additionally acknowledges Koniwa (CC BY 3.0) and the SIWIS corpus (CC BY 4.0) as training-data sources, with links to their original publications. The model card and license are preserved with the authoring resources.

Frostwatch uses Unreal® Engine. Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere. Unreal® Engine, Copyright 1998–2026, Epic Games, Inc. All rights reserved.

## Parchment and book typography — 16 September 2026

**Cormorant Garamond**, designed by **Christian Thalmann**, Copyright 2015 The Cormorant Project Authors. Source: https://github.com/google/fonts/tree/main/ofl/cormorantgaramond . Licensed under the **SIL Open Font License 1.1**. Frostwatch embeds a static medium-weight instance renamed **Frostwatch Story Serif**; the original copyright is retained. The full licence accompanies the game in `Licenses/Cormorant-OFL.txt`. The original font, licence, transformation script and source/output hashes are retained in the authoring project. No paid font service was used. The parchment, book, ornaments and animation are drawn by the game's interface code.

## Village livestock — 18 September 2026

**0 A.D. livestock**, **Wildfire Games** (https://www.wildfiregames.com/), provides the cattle and pig meshes, skins, and idle, walking and feeding animations. Source: https://github.com/0ad/0ad/tree/master/binaries/data/mods/public/art . The cattle sources identify **Daniel Morgado**, and the pig sources identify **Stanislas Daniel Claude Dolcini**, as their authors. These assets are licensed under **Creative Commons Attribution-ShareAlike 3.0 Unported**, https://creativecommons.org/licenses/by-sa/3.0/ . Frostwatch's adaptations repair legacy COLLADA texture semantics, smooth the silhouettes, convert the rigs and animations to FBX, and adjust materials, scale and playback speed for Unreal. Those adapted assets retain CC BY-SA 3.0; they are not covered by the restrictions on unrelated Fab content.

**Sheep (rigged, textured and animated)** by **p0ss**, https://opengameart.org/content/sheep-rigged-textured-and-animated , supplies the sheep mesh, packed texture and original animation clips. The creator credits the boar base texture to **titus tscharntke**, from https://opengameart.org/content/woodland-animals-texture-pack . Licensed under **CC BY-SA 3.0**, https://creativecommons.org/licenses/by-sa/3.0/ . Frostwatch selects one wool variant, reconnects the packed texture, converts the rig and three animations to FBX, and adjusts the material and scale. The adapted sheep assets retain the same licence. These creators do not endorse Frostwatch.

No animal assets were purchased.

## Reused cave and pool assets — 0.5.9

The buried entrances and ridges reuse **Icelandic Boulder by Quixel/Megascans**, already acquired with the owned Medieval Village scenery. **Water Materials by tharlevfx** supplies the stone colour/normal textures used by the adapted cave material and the existing waterfall/water effects. **Procedural Nature Seasons Pack by PurePolygons** supplies the existing surrounding ground and snow textures. Frostwatch adds its own generated tunnel meshes, terrain shaping, material adaptation, placement, collision and swimming logic. These adaptations do not change the original assets' applicable Fab/Epic licence terms. No new purchase was made for this pass.

## Unreal combat adaptations - preview 0.5.10

Frostwatch bakes detached heads, left/right arms and left/right legs from the existing Greystone, orc and DemonLord2 meshes, preserving their material assignments. These derived parts retain the respective original asset licenses above and are distributed only as cooked game content. Joint caps, ballistic droplet motion and the glossy droplet material are project-authored. No new third-party assets or recordings were acquired for this pass.
