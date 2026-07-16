# KickForge — Architecture et notes pour le développement

Document de référence pour les instances Claude Code (et humains) travaillant sur ce
repo. Les specs fonctionnelles sont `BRIEF-KICKFORGE.md` (v1) et `BRIEF-KICKFORGE-V2.md`
(v2) ; ce document décrit le code tel qu'il est, les conventions prises quand les briefs
ne tranchaient pas, et les pièges rencontrés (tous couverts par des tests).

## 1. Flux de signal (v2)

```
MIDI note-on / bouton Play / loop de pré-écoute
   │
   ▼  KickEngine (src/dsp/KickEngine.*) — orchestrateur, rend DEUX bus mono
   ├─ AttackLayer  : bruit xorshift → LP one-pole (atkTone) → decay exp (atkDecay)
   ├─ KickVoice    : Osc A/B (PolyBLEP sur sqr/saw) × pitch env exp × amp env
   │                 → DriveStage du punch (waveshaper, oversampling ×2, tone LP)
   │                 → punchLevel   ← l'attack est ajouté APRÈS ce drive
   └─ CrunchLayer  : osc sin/tri à fréquence FIXE (pitchEnd × 2^(tune/12))
                     × env (attack lent + long decay) → SA DriveStage → crunchLevel
   │
   ├── busAP = attack + punch  ──────────────► FxStage : send de REVERB (wet only)
   └── mix = busAP + crunch                        │ le crunch ne touche jamais la reverb
        │                                          │
        ▼ chaîne commune mono                      │
   FilterStage (SVF TPT LP/HP/BP) → EqStage (3 peaks RBJ) → DistStage (parallèle)
        │                                          │
        ▼ duplication mono→stéréo                  │
   FxStage : chorus (global) puis  + wet reverb ◄──┘
        ▼
   CompStage (macro, GR meter atomic) → ClipStage (soft/hard + ceiling)
        → WidthStage (mid/side, crossover LR2 150 Hz, sub mono, clamp au ceiling)
        → gain de sortie (LinearSmoothedValue) → sortie stéréo
```

Le wet de reverb est réinjecté **avant** le compresseur : comp/clip/width traitent
aussi la queue (le width en tire sa stéréo — le signal synthétisé est mono à la source).

## 2. Organisation du code

| Emplacement | Contenu | Dépendances |
|---|---|---|
| `src/dsp/KickVoice`, `AttackLayer` | sources pures (oscillateurs, enveloppes) | **aucune** (lib statique `kickforge_dsp`) |
| `src/dsp/*Stage`, `CrunchLayer`, `KickEngine` | étages et orchestrateur | `juce_dsp` — compilés dans le plugin ET dans `dsp_tests` via `KICKFORGE_JUCE_DSP_SOURCES` |
| `src/presets/GenrePresets.h` | 5 presets constexpr, 48 paramètres chacun (tout sauf `genre`/`outputGain`) + `randomizedNormalised` | aucune |
| `src/presets/StateMigration.h` | migration d'état `kickforge_params_v1` → `v2` (tag + `punch`→`atkLevel`) | `juce_core` |
| `src/export/WavExporter` | rendu offline chaîne complète → WAV 48 kHz/24 bits | `juce_audio_formats` |
| `src/ui/` | LookAndFeel (palette maquette v3), SectionPanel + widgets (LabelledKnob, SegmentedControl, meters, minis), WaveformDisplay, EqCurveDisplay, PresetBar, UiScaling.h (maths pures testées) | `juce_gui_basics` |
| `tests/dsp_tests.cpp` | ~70 TEST_CASE Catch2, console app JUCE | tout sauf l'UI (sauf `UiScaling.h`) |

`PluginProcessor` lit les atomics APVTS une fois par bloc (`currentXxxParams()`),
pousse les structs `Parameters` dans les étages, et gère : presets par genre
(listener sur `genre` → `AsyncUpdater` → application sur le message thread, avec
garde `suppressPresetLoad` pendant `setStateInformation`), Play (`playRequested`
atomic consommé en tête de bloc), loop de pré-écoute (2 atomics → moteur),
vu-mètres (`outputPeak`, GR du comp).

## 3. Contrat temps réel et threading

La section 9 du brief v1 est non négociable : aucune allocation/lock/I-O dans
`processBlock`. Concrètement dans ce code :

- Tout est pré-alloué dans `prepare()` (scratchs des étages, `busScratch` du processeur).
- L'EQ recalcule ses coefficients RBJ **en place** (`updateBandCoefficients`) —
  ne PAS remplacer par `makePeakFilter`, qui alloue.
- Audio → UI : uniquement des atomics (`getGainReductionDb`, `getOutputPeak`).
- UI → audio : atomics (`playRequested`, `loopEnabled`, `loopIntervalSeconds`).
- `WaveformDisplay` rend sur le **message thread** avec ses propres instances DSP
  (moteur recréé à chaque rendu — allocations permises là), à partir d'une copie
  des paramètres, throttlé 30 fps (timer + drapeau `dirty` atomique levé par les
  listeners APVTS). Deux passes déterministes isolent les couches : passe complète
  (busAP, mix brut) puis passe attack-muté ; attack = busAP − punchSeul,
  crunch = mixBrut − busAP. Le déterminisme repose sur la graine xorshift fixe et
  des moteurs frais — ne pas casser ça.
- Note-on **sample-accurate** : `processBlock` segmente le rendu aux positions MIDI ;
  le moteur segmente lui-même aux points de trigger du loop et aux fins de fade.

## 4. Patterns établis (à respecter dans tout nouveau code)

- **Cycle de vie d'un étage** : `prepare(spec)` → `setParameters(p)` → `reset()`.
  Les `reset()` (les nôtres et ceux de JUCE) **snappent les smoothers sur la cible
  courante** — donc toujours pousser les paramètres AVANT de reset. L'inverser
  produit des rampes fantômes depuis les défauts (bug réel : gain de drive 9×
  pendant 20 ms au premier bloc ; mix chorus 0.5 pendant 50 ms).
- **Couches additives** : `renderAdd()` AJOUTE au buffer (contrat de bus).
- **Extinction sans clic** : `beginQuickRelease(n)` — rampe linéaire de l'enveloppe
  de la SOURCE, avant sa non-linéarité. Ne jamais fader un bus post-drive : la
  discontinuité de retrigger traverse l'oversampler à gain plein (bug réel, saut 0.28).
- **Paramètres par note vs continus** : les paramètres de source (pitch, enveloppes,
  formes d'onde, niveaux de couche) sont figés au `trigger()` ; les paramètres de
  traitement (gains, cutoffs, mixes) sont lissés ~20 ms (`SmoothedValue`, par
  échantillon pour les gains, par bloc pour les cutoffs).
- **Branchement au niveau de la voix** : l'Osc B off ne coûte rien
  (`renderLoop<bool>` template) — exigence du brief, à conserver.
- **UTF-8** : tout littéral non-ASCII destiné à `juce::String` passe par
  `juce::String::fromUTF8` (le constructeur `char*` interprète en Latin-1).
- **Portabilité MSVC** (la CI Windows est là pour ça) : `#include <algorithm>`
  explicite partout où `std::min/max/copy/fill` sont utilisés ; pas de `M_PI`
  (constante locale `kPi` dans les tests).

## 5. Paramètres et compatibilité d'état

- APVTS `kickforge_params_v2`, 50 paramètres. Les nouveaux (couches) ont un
  version hint `ParameterID{id, 2}`.
- `setStateInformation` appelle `presets::migrateStateToV2` (no-op si déjà v2) :
  tag renommé, `punch` → `atkLevel`, nouveaux paramètres aux défauts
  (`crunchLevel` 0 ⇒ un projet v1 sonne quasi identique). **Ne jamais casser cette
  migration** — des projets DAW utilisateurs en dépendent. Toute évolution future
  du schéma = `kickforge_params_v3` + nouvelle branche de migration + tests.
- Charger un preset écrase tout sauf `genre` et `outputGain`. Le test
  « les 5 presets couvrent tous les paramètres » casse volontairement si un
  paramètre est ajouté sans mettre à jour `GenrePresets.h` (48 attendu).

## 6. Conventions prises hors brief (validées par Benjamin, ne pas re-trancher)

- Pitch env : τ = `sweepTime` tel quel. Enveloppes : `decay`/`atkDecay`/`crunchDecay`
  = temps de chute de **−60 dB** ; extinction de voix à −80 dB.
- Drive : gain = `30^(amount/100)` (exponentiel). Tone = SVF LP 12 dB/oct Q 0.707.
- Filtre : réso 0–90 % → Q exponentiel 0.707→8 (pas d'auto-oscillation).
- Reverb : compensation des facteurs internes de freeverb dans `juce::Reverb`
  (`dryScaleFactor=2`, `wetScaleFactor=3`) — send wet-only = `wetLevel = mix/3`,
  `dryLevel = 0`. Caractère fixe : roomSize 0.55, damping 0.5 ; chorus 1.2 Hz/0.3/7 ms.
- Attack (ex-punch v1) : bruit xorshift graine fixe `0x2545f491`, échelle crête 0.6.
- Mix Osc B compensé : `(A + l·B)/(1+l)`. Crunch : level POST-drive.
- Le ceiling du clipper est re-clampé en sortie de WidthStage (`safetyCeilingDb`,
  alimenté par `clipCeiling`) — le crossover peut faire ré-émerger ~0.2 dB.
- Crêtes > 1.0 en sortie de drive = ringing de Gibbs du downsampling sur du contenu
  carré : physique, pas un bug ; le clipper final gère le plafond.
- Loop de pré-écoute (hors brief, demandé par Benjamin) : intervalle NON-APVTS
  (pas automatisable, pas persisté), coupé à la fermeture de l'éditeur.

## 7. Tests

Un seul binaire Catch2 (`dsp_tests`, console app JUCE). Helpers réutilisables en
tête de `tests/dsp_tests.cpp` : `renderSamples`, `renderBuses`, `neutralEngineParams`
(attack/crunch off, drive transparent), `processThrough` (mono avec warm-up des
smoothers), `processThroughFx`/`processStereo` (stéréo), `goertzelMag` (Hann,
comparaisons relatives uniquement), `rmsOf`, `correlation`, `steadyToneParams`.

Leçons méthodologiques payées cash :
- Les enveloppes sont à −60 dB au temps de decay : caler les fenêtres de mesure
  dans du signal vivant (plusieurs tests ont « échoué » à cause de fenêtres mortes,
  dont une mesure de fréquence polluée par les résidus du drive dans le silence).
- Le bruit étant déterministe, on isole une contribution par **différence de deux
  rendus** — c'est le pattern de vérification le plus puissant du repo.
- pluginval strictness 10 ouvre l'éditeur et fuzze les paramètres : c'est le filet
  pour l'UI et le chargement de presets asynchrone.

## 8. Build, CI, publication

- JUCE 8.0.14 via CPM (pinnée dans `CMakeLists.txt`), Catch2 3.15.2.
  `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`.
- CI (`.github/workflows/build.yml`) : Linux + Windows, ctest + pluginval
  strictness 10, et packaging du bundle Windows (`KickForge.exe` + `KickForge.vst3`
  + `packaging/LISEZMOI-TEST.txt`) en artefact `KickForge-windows` — c'est la seule
  façon de produire un binaire Windows (pas de cross-compilation JUCE depuis Linux).
- Publication : repo public `Bebop-Tech/kickforge`, AGPLv3 (imposé par le tier
  open source de JUCE 8). Compte `gh` : **Bebop-Tech, en HTTPS** — le credential
  helper suit le compte actif, vérifier `gh auth status` avant de pousser.
- Latence déclarée au host : celle de l'oversampler du drive du punch (~1.6 éch.).

## 9. État et suites possibles

Fait : v1 complète (10 étapes du brief) + v2 3 couches (8 étapes de la spec v2) +
loop de pré-écoute + visuels par couche. Testé : 70+ cas, pluginval 10 sur les
deux OS. En attente côté utilisateur : retour d'écoute du frère (testeur), test
FL Studio sur machine Windows (section 10 du brief v1), affinage des presets à
l'oreille (section 5 de la spec v2 = points de départ). `keyTrack` (la note MIDI
transpose pitchEnd) existe dans l'APVTS mais n'est **pas encore câblé** dans le
moteur — seul reliquat v1 non implémenté.
