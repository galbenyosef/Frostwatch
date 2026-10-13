# Fighting — Unreal preview 0.5.10

The combat update runs in the native Unreal Engine 5.8.2 C++ game. It retains the campaign, buried cave network, swimming, livestock and existing save format.

## Aim and injuries

Aim the crosshair at a head, torso, arm or leg. Wounds are located against the animated character, with accumulated injury tracked separately for the head and each limb during an encounter. Head hits deal more damage. Arm wounds reduce attack damage; leg wounds slow pursuit. A strong strike can remove a sufficiently damaged arm, leg or head. Sword-arm removal hides the weapon and reduces both attack strength and reach. Head and leg removal are fatal. A normal encounter reset restores all limbs, health and mobility.

All four humanoid character meshes have matching detached heads, left/right arms and left/right legs. Detached parts use physics, and capped wounds follow the surviving skeleton. Dragon and story wards retain their existing rules; friendly characters cannot be injured.

## Moves with the default controls

| Input | Action |
| --- | --- |
| Left mouse | Alternating cuts |
| A or D + left mouse | Choose the cut direction |
| R | Overhead heavy cut |
| W + R | Driving thrust, with a longer reach and narrower contact |
| S + R | Rising cut |
| Ctrl to crouch, then left mouse | Low cut; look down to target a leg |
| Hold right mouse + left mouse | Shield bash; follow quickly for a riposte |
| Left mouse → left mouse → R | Combination finisher |
| X during early wind-up | Feint, with an additional endurance cost and short recovery |
| Right mouse as the enemy blow arrives | Parry, opening a riposte window |
| Left Alt | Dodge |

Buffer one follow-up after the current swing starts. The direction is captured when the follow-up is pressed. Every move spends endurance; attacks cannot bypass broken-guard or feint recovery. Walls stop the blade. Shield bashes stagger but cannot cut or remove limbs. Movement/action remapping also changes the combinations; **Pause → Fighting techniques** explains them in the game.

## Motion and blood

Swing anticipation, cutting arcs and recovery blend continuously through the first-person sword pose. Thrusts, rising cuts, low cuts and shield bashes have distinct motion and contact paths. A strong contact briefly holds the blade to give the blow weight.

Glossy blood droplets stretch with velocity, fall under gravity and collide with scenery. Surface marks spread and fade; a severed joint emits a short trail. Effects are bounded to 64 droplets and 12 ground marks per affected enemy, with timed expiry and reset cleanup. This is an animated ballistic effect, not a volumetric liquid simulation.

## Verification

The Unreal editor and Win64 game targets compiled successfully. The packaged executable passed **396 checks**: 212 combat/contact/injury checks, 126 campaign checks, 53 runtime checks and five swimming checks. Coverage includes all five detachable parts across four humanoid meshes, nonfatal arm injury, equipment hiding, leg collapse, reset cleanup, input combinations, buffered attacks and feints.

A scripted live fight recorded two successful parries, defeated the enemy and preserved player health at 100. The fighting guide, live action, surviving enemy after arm removal and leg-removal collapse were visually inspected. These captures used the Performance preset at 1280 × 720, with the guide additionally checked at 960 × 540. Their measured windows reported no rendering errors or GPU-budget exceedance; this is not a whole-game performance guarantee.

The body-part authoring pass verified 20 detached meshes and left the existing Hearthmere map byte-for-byte unchanged. Automated scenarios do not establish that every fight feels realistic or replace a human playthrough. Broader animation polish and full campaign acceptance remain open.

Review the [in-game guide](Screenshots/fighting-guide.png), [live action](Screenshots/fighting-live.png), [nonfatal arm removal](Screenshots/fighting-arm.png) and [leg collapse](Screenshots/fighting-leg.png).
