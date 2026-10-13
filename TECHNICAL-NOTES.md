# Technical notes — Frostwatch preview 0.5.10

Frostwatch is an offline, single-player Windows fantasy adventure and an AI-led development experiment. This document describes the implemented preview, not a promise that every feature has production-game polish. See [AI development and measured resource use](AI-DEVELOPMENT.md), [performance](PERFORMANCE.md), [credits](CREDITS.md) and [distribution terms](DISTRIBUTION.md).

## Platform, architecture and development tools

- Current engine: Unreal Engine 5.8.2; Windows x64 Development build, DirectX 12 / Shader Model 6. Unreal Editor is not required by players. The predecessor used Godot 4.7.2; Godot rendering settings do not describe this Unreal build.
- Native C++ game module with engine input, JSON data, procedural mesh support, AI/navigation modules and RHI diagnostics. Editor-only authoring uses Unreal Python, editor scripting, asset import and generated static meshes.
- Data-driven world, quest/item/enemy definitions and terrain height samples accompany the authored Hearthmere map. Runtime systems cover character control, combat, quests, dialogue, settlements, travel, weather, menus and saves.
- Development automation uses Python and PowerShell; UnrealBuildTool builds the game/editor and AutomationTool cooks/stages packages. Iterative cooking, limited build parallelism and working caches on the development HDD reduce redundant work and SSD pressure.
- Git tracks source, configuration templates and authoring scripts locally. The public repository publishes player documentation, screenshots, a downloadable runtime and the project-authored [Unreal C++ source snapshot](Unreal/README.md). It does not expose the complete licensed source-asset library or Unreal Engine source.

## Rendering and memory

- Balanced retains Lumen lighting while using conventional shadows to reduce cost on the GTX 1060. High and Epic retain virtual shadow maps. This is deliberate per-preset selection, not an assertion that newer rendering features are always faster.
- Performance/Balanced use temporal anti-aliasing; higher presets use temporal super resolution. Resolution, fullscreen/window mode, render scale, frame cap and quality are player settings. The 0.4 tests used 720p; they are not 1080p performance claims.
- Explicit texture and Nanite geometry streaming budgets, temporary streaming allocations and lighting/render-target limits prevent the 3 GB GPU from being overcommitted. Runtime code overrides some scalability defaults; Balanced's effective texture budget is 384 MiB.
- Balanced permits finer texture mip levels than the prior version, with anisotropic filtering. Larger source textures do not mean all full-resolution texture data is resident at once.
- Instanced foliage, distance culling, terrain tiles and reduced distant wind deformation bound outdoor rendering work. Nearby foliage remains animated; Performance/Balanced stop foliage wind deformation beyond 80 metres.
- A 256 MiB packaged-asset read cache and 32 MiB read buffers use system RAM. The game already uses system RAM for CPU-side world/assets. This is not a RAM disk or an expansion of equally fast VRAM; GPU spill into shared memory can hurt performance. No isolated FPS gain is claimed for the cache.
- Diagnostics record mean/p95 frame times, graphics-memory usage/budget, texture streaming pressure, process RAM, live-world status and render errors. The memory budget is supplied by Windows and can vary with other applications.

## World, scenery and weather

- Authored terrain and regional dressing distinguish southern woodland, damp Glasswater, the snowy north and darker dragon territory. Some transitions and repeating surfaces remain visibly unfinished.
- Scenery combines owned Fab resources, credited CC0 assets and project-authored geometry/materials. Imported source art is converted to runtime assets and selectively cooked.
- The waterfall uses tharlevfx Water Materials base/arc meshes, tuned translucent water materials, foam/splash particles, a headwater surface, scanned rock surrounds, an irregular receiving pool and a basin carved into the terrain. It is an animated visual effect, not a fluid simulation. Demonstration walls/platforms are excluded.
- Sky, sun/moon and weather use a project controller and selected licensed sky/cloud resources. Fountains, local lights and night-time castle lighting are integrated, with screenshot checks in the courtyard and hall.
- Quest locations include towns, inns, caves, castles and travel camps. Procedural/authored collision, road clearance and interaction reachability are validated automatically; that is not a substitute for testing every possible player route.

## Character movement, NPCs and travel

- First-person movement includes walking, sprinting and jumping. Horse mounting, galloping and cart travel are available through world interactions.
- Townspeople use daily routines, walking routes and work interactions. This approximates village activity; it is not a fully simulated economy or autonomous generative-agent society.
- Character rigs and animations include licensed/retargeted clips. Civilian locomotion uses the credited Quaternius animation library. Some rigs, clips and procedural behaviours are reused.
- The two dragons use the owned PROTOFACTOR Mountain Dragon rig and original animation clips with separate material colours. Fire uses animated effects with softened billboard edges and depth fading, not volumetric combustion.

## Combat, death and respawn

- Light/heavy attacks, blocks, timed parries, dodge/recovery timing, attack buffering and chained directional cuts form the melee system. Recorded swing, metal contact and impact samples replace earlier synthetic combat tones.
- Selected finishing blows can sever prebuilt head/arm pieces. Blood/wound effects and animation/ragdoll responses do not constitute arbitrary mesh slicing or fully cinematic executions.
- Ordinary guards/orcs respawn after 24 campaign hours only when sufficiently distant from the player and outside visible arrival conditions. Checks include 80 metres from home and 25 metres from the corpse. Named bosses remain defeated, and quest defeat history is preserved.
- Settled corpses update less frequently to reduce unnecessary work. Save migration preserves older progression data.

## Campaign, interface and persistence

- Ten journal chapters, a guided opening, world map, interaction prompts, inventory/rewards and menus provide the adventure structure. The Last Ember story connects Edrin, supplies, the three oaths, Ste, Stu and the returned watchfire. Progress-aware dialogue and aftermath preserve existing save IDs and completion gates.
- A Canvas-rendered animated book presents the opening, found texts and journal; NPC dialogue slides up on parchment. A bundled Cormorant-derived serif face has its full SIL Open Font License and creator credit included. Long passages paginate with keyboard/mouse page controls; animations use real time while gameplay is paused.
- Castle detailing uses seven instanced groups for paving, vault ribs, standards and the Crown ward, plus eleven bounded spotlights, two with shadows. The lantern directs a bounded, wall-aware spotlight toward the path and reduces close-range glare. Castle architecture remains unfinished.
- Keyboard/mouse controls, rebinding, fullscreen/lower resolutions and quality settings are exposed in the interface. See README and the in-game controls screen for bindings.
- Saves reside in Frostwatch/Saved/SaveGames; settings are under Frostwatch/Saved/Config. Release ZIPs contain no personal save or settings files. Back up Saved before manually upgrading.
- Verification captures restore settings byte-for-byte and check save hashes. Local installation retains the previous executable/content packs for manual rollback.

## Audio and AI boundary

- Water/stream ambience is positional; music, footsteps, horse hoof sounds, combat contact, pain/death sounds and character dialogue use their credited sources and runtime playback/mixing.
- Current dialogue is pre-rendered, subtitle-matched Kokoro v1.0 neural speech with seven selected voice profiles/blends. It is synthetic speech, not hired actors. The model and authoring tools are not shipped.
- The game does not call a language model to generate NPC conversation during play. Dialogue and quests are authored; the player needs no AI account, API key or internet connection.
- Historical Piper narration and Godot-era notices are retained as provenance in the credits, but the old Piper tale recordings are absent from the 0.4 cooked asset inventory.

## Testing and publication

- The 0.5.1 package passed 71 campaign, 50 runtime and 72 combat/contact checks (193 total). The installed version repeated all 121 campaign/runtime checks. Both existing saves and settings remained byte-identical during installation.
- Current castle/lantern captures passed memory/render-error acceptance. Performance results and screenshots are in PERFORMANCE.md. These captures use stationary cameras, controlled weather and frozen NPC simulation. Long-session travel and active castle combat measurements remain outstanding. Book/page animation and text fit were checked separately, including a diagnostic second page and three viewport resolutions.
- Packaged content uses Unreal Pak/IoStore files. Runtime payload hashes match the verified local installation. ZIP integrity is checked and SHA-256 checksums accompany the download; GitHub upload digests are verified before publication.
- The download includes the runtime, launcher, Visual C++ redistributable and player/technical/credit documents. Editor tools, source art, personal saves/settings, debug symbols and build logs are excluded. Cooked configuration checks confirm private editor settings are absent.
- This is an unsigned development preview. It has been measured on one laptop; broad GPU/driver/platform compatibility, long-session stability and exhaustive playthrough coverage are not established. No multiplayer, console, Mac, Linux, mobile or browser build is supplied.

## Known limitations and interpretation

Landscape repetition, some shoreline geometry, grove roots/branches, reused animation and general composition still need work. A passing automated test establishes its specific assertions, not that the game is bug-free, balanced or AAA quality. The human review repeatedly exposed visible problems that automated checks missed. Frame rates vary with location, action, temperature, drivers and competing workloads. All third-party creators retain the rights stated in their licences.

## 0.5.2 readiness batch

Humanoids use a native single-sequence animation proxy with short blends from the last displayed local-bone pose. The incoming clip advances immediately; there is no second sequence evaluation. Compact-pose snapshots are invalidated when the required-bone mapping changes. Locomotion transitions use 180 ms and actions/reactions 90 ms. Dragons retain their previous animation player. This improves pose continuity; it is not a complete motion-matching system or a replacement for authored contact review.

Reading settings persist in GameUserSettings.ini. Story-body sizes of 100/125/150 percent rewrap and paginate; reduced motion removes the page and dialogue-slide animation. Headings, chapter-selection controls and the general HUD are not globally resized by this option. Separate audio-volume and full controller work remain on the roadmap.

Capture diagnostics additionally retain up to 120,000 post-warmup frame samples and report p99, worst frame, counts above 50/100 ms and peak dedicated-memory demotion. These metrics remain diagnostic-only. See PERFORMANCE.md for the actual moving test and its limits.

## 0.5.3 save reliability and combat feedback

The original expedition schema and primary slot remain unchanged. Save writes serialize through Unreal's SaveGame API, verify the previous-generation slot before replacing the primary, then compare primary readback bytes. A missing/unreadable primary falls back to the `_Previous` slot. That can lose the latest changes; it does not guarantee survival of hardware failure affecting both copies. BeforeNewGame and BeforeImport snapshots are verified before replacement proceeds. Failure messages, retry and explicit quit-without-saving prevent a false success report.

Save-store injection tests exercise real expedition serialization against controlled missing, unreadable and failed-write cases without using player save slots. Compact combat cues are separate from quest notifications. Pause/title spacing adapts to viewport height. No new paid assets, audio, models or external services were added.

Performance/Balanced limit concurrent texture replacement memory to 16 MiB instead of 32 MiB; the 384 MiB streaming pool and texture quality settings remain unchanged. High/Epic retain their 64 MiB temporary budget. Native platform-slot tests use unique diagnostic names and retain their files outside the player save folder.

## 0.5.4 persistent world consequences

A per-world cache associates the two existing supply interactables with their existing crate actors by mesh and proximity. Quest flags select uncollected, carried or delivered presentation; no additional scenery is spawned. Delivered props use static-world traces and their mesh bounds to align their base with the ground. Hidden collection targets are excluded from interaction and HUD prompts.

Edrin's rescued flag reconstructs his Hearthmere placement on loading. During play, relocation waits until the player is outside the prison area and has closed menus. His inn route is computed once against static collision in a bounded Hearthmere grid and cached for reuse, with extra capsule clearance and a floor-height path through the west doorway. Fresh expeditions reset original transforms and routines. No save schema migration or new art/audio purchase is required. The new crate inspection text has no NPC recording; existing character speech is retained.

## 0.5.5 castle structure

Six authored mesh actors reuse owned materials with metre-scale UVs. The gate alone is movable; its open/closed transform is derived from existing jailer/rescue records and changes only when state changes or restores. Capsule-sweep tests verify blocking and entry clearance. Geometry authoring is repeatable and backs up the original map. No save schema changes or new lights.

Castle light components and their night intensities are cached once by the existing weather actor. Exterior lamps and accents follow its smooth daylight factor; interior lights remain enabled. Prison lamp power/radius and the exterior clue light are bounded separately to reduce the overexposure found in the initial captures.

## 0.5.6 roads and verges

The existing masked road material uses temporal coverage for its edge, four texture samples (two albedo, two normal) from the existing rocky-soil set, rotated/scaled world coordinates, wheel-track roughness and low-frequency colour variation. Balanced/Performance use TAA; higher presets use TSR. The existing 2048-pixel ground mask has a wider blurred wear channel. Four instanced mesh groups add 200 decorative plants/stones around the castle. No new lights, texture sets, terrain-height changes, route changes, save-schema changes or C++ changes.

## 0.5.7 water and cave construction

Rounded cave shells are generated from the existing connection graph using an implicit surface and marching tetrahedra, split into independently bounded sections: 270,096 triangles across the three areas. Triplanar rock colour/normal sampling uses existing pack textures. New collision follows the visible shells; the established floor and old backing collision are retained. A carved river modifies 558 heightfield samples; the bridge retains road travel. Existing foliage is fitted from retained original transforms to avoid repeated offset drift. The hidden passage table is staged as game data; travel validates proximity, combat/mount state and a floor/capsule-safe destination before moving. Discovery uses the existing flag list without a save schema change.

## 0.5.8 village livestock

Eighteen AFWLivestock actors share three skeletal models and nine animation sequences. Each animal chooses short goals within 1.1 metres of its home, turns before walking, checks world-static floor height/slope and sweeps its capsule when moving. Cattle use a wider capsule. Authoring places animals clear of the original farmer work routes and computes additional tending approaches with static-collider and herd-home clearance.

Nearby livestock update normally; beyond 90 metres movement pauses and actor logic runs once per second. Skeletal poses update only when rendered, with update-rate optimisation enabled. Textures are capped at 1024 pixels. No new lights, save schema or player-data migration. Herd positions and ambient animation states reset when the world loads.

Working herders/farmers pause near their route's livestock, face the nearest animal and trigger its feeding animation. A tending visit is counted once until the previous visit expires, not once per frame. Human tending currently reuses the standing idle pose; there is no bespoke hand-feeding animation, production economy or animal interaction menu. Existing evening/home routines and stable-hand horse recovery remain.

Blender converts the credited CC BY-SA source assets; Unreal imports the skeletons, clips and explicit skeletal materials. Public Assets/Livestock contains the reusable adapted FBX files and textures, also included in the download. Unrelated licensed game assets remain subject to their own terms.

## 0.5.9 buried caves and swimming

The new implicit-surface tunnel network contains 384,545 triangles in 124 separately bounded mesh sections. Its continuous floor follows the closest route centreline; this avoids the steps created by unioning independently capped sloping floors. Authored terrain rises above the existing caves, with bounded openings where the terrain intersects entrances. Runtime terrain tiling preserves those missing triangles and keeps its existing collision surface. Original rectangular cave backing collision is disabled, and the retained puzzle floor supplies standing collision. Clearance probes ignore movable enemies guarding entrances; permanent geometry remains included, and enemy contacts are tested separately in the combat suite.

New tunnel sections retain their walls without distance culling, use a 0.5 distance-field resolution scale and at most two Lumen mesh cards per section. Ordinary occlusion can still reject hidden sections. Performance and Balanced reserve 32 MiB for Nanite streaming instead of 64 MiB; their texture budgets and lighting settings remain unchanged. Eleven local junction lights cast no shadows and have short draw ranges. 30 formations reuse the credited Icelandic Boulder asset to break up exposed slopes. Embedded, overlapping scanned rocks screen the cut tunnel mouths, and continuous approach floors reuse the surrounding terrain material. These decorative formations do not contribute distance-field lighting. Local steep mountain faces blend in triplanar bedrock while keeping snow/soil on gentler ledges. A winding submerged stone shelf replaces the straight waterfall approach. A footstep trace resolves underground stone from the actual floor rather than the landscape's XY surface map.

AFWWaterVolume uses an overlapping box broad phase and the visible pool's irregular elliptical boundary. Native character movement handles swimming and collision; a bounded upward movement input supplies surface flotation. Visual water/foam meshes explicitly use NoCollision and actor collision is disabled. No player repositioning is used to swim out or travel between caves. Diagnostic drivers place their initial player once, then use normal movement input; they are not enabled in ordinary play.

No save schema changes. Save loading retains the existing camp-checkpoint behaviour. The source generation and authoring scripts retain backups and original foliage transforms for repeatable placement.

The 0.5.10 combat changes and their verification are described in [FIGHTING.md](FIGHTING.md).
