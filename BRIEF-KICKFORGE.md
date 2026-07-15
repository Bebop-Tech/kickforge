# KickForge — Brief technique pour Claude Code

## 1. Contexte et objectif

Développer **KickForge**, un plugin audio VST3 de génération de kicks par synthèse, destiné en priorité à FL Studio (Windows) mais compatible avec tout DAW hébergeant des VST3. L'utilisateur choisit un preset de genre (Techno, Hard Techno, Hardstyle, EDM, Trance), le plugin génère un kick complet qu'il peut ensuite sculpter paramètre par paramètre : forme d'onde, pitch envelope, enveloppe d'amplitude, drive, EQ 3 bandes, distorsion parallèle, reverb, chorus, compresseur, clipper final et largeur stéréo.

Le plugin est un **instrument** (il reçoit du MIDI et produit de l'audio) : chaque note MIDI déclenche le kick, la hauteur de la note pouvant optionnellement transposer le pitch de fin.

**Objectif de la v1 :** un kick audible, crédible par genre, avec l'UI complète et l'export WAV. Reverb et chorus peuvent être livrés en v1.1 si le temps manque.

---

## 2. Stack technique

| Élément | Choix |
|---|---|
| Framework | JUCE 8.x (récupéré via CPM ou FetchContent CMake, pas de Projucer) |
| Build | CMake ≥ 3.22, C++20 |
| Formats | VST3 (+ Standalone pour itérer vite sans DAW) |
| OS de dev | Linux (Fedora) — build Standalone pour tester au casque |
| OS cible | Windows x64 (build final sur machine Windows ou CI GitHub Actions) |
| Tests | pluginval (niveau strictness 5 minimum) + Catch2 pour les unités DSP |
| Gestion des paramètres | `juce::AudioProcessorValueTreeState` (APVTS) exclusivement |

**Important :** ne pas utiliser le Projucer. Projet 100 % CMake, avec un `CMakeLists.txt` racine qui fetch JUCE. Prévoir une CI GitHub Actions qui build Linux + Windows dès le début — ça évite les surprises de portabilité.

---

## 3. Arborescence cible

```
kickforge/
├── CMakeLists.txt
├── cmake/            # helpers (CPM, warnings)
├── src/
│   ├── PluginProcessor.h/.cpp     # AudioProcessor, APVTS, chaîne DSP
│   ├── PluginEditor.h/.cpp        # UI racine
│   ├── dsp/
│   │   ├── KickVoice.h/.cpp       # 2 oscillateurs (A corps + B couche) + pitch env + amp env
│   │   ├── KickEngine.h/.cpp      # gestion des déclenchements MIDI, retrigger
│   │   ├── DriveStage.h/.cpp      # waveshaping (soft/hard/fold) + tone filter
│   │   ├── FilterStage.h/.cpp     # filtre multimode SVF (LP/HP/BP), cutoff + résonance
│   │   ├── EqStage.h/.cpp         # 3 bandes entièrement paramétriques (freq/gain/Q)
│   │   ├── DistStage.h/.cpp       # distorsion parallèle (tube / fuzz / bitcrush) avec mix
│   │   ├── FxStage.h/.cpp         # reverb + chorus (juce::dsp)
│   │   ├── CompStage.h/.cpp       # compresseur macro (threshold+ratio+makeup liés) + attack
│   │   ├── ClipStage.h/.cpp       # clipper final soft/hard + ceiling
│   │   └── WidthStage.h/.cpp      # mid/side, width appliqué au-dessus de 150 Hz (sub mono)
│   ├── ui/
│   │   ├── LookAndFeel.h/.cpp     # knobs rotatifs custom, palette sombre
│   │   ├── WaveformDisplay.h/.cpp # rendu du kick généré + courbe de pitch
│   │   ├── SectionPanel.h/.cpp    # cadre de section réutilisable
│   │   └── PresetBar.h/.cpp       # boutons de genre
│   ├── presets/
│   │   └── GenrePresets.h         # valeurs par genre (constexpr / static)
│   └── export/
│       └── WavExporter.h/.cpp     # rendu offline du kick → WAV
├── tests/
│   └── dsp_tests.cpp
└── .github/workflows/build.yml
```

---

## 4. Architecture DSP — chaîne de signal

```
MIDI note-on
   │
   ▼
Osc A (corps : sin / tri / carré / saw, PolyBLEP) ──┐
  fréquence pilotée par la PITCH ENVELOPE            ├─ mix (Osc B : level + tune en demi-tons,
Osc B (couche : mêmes formes d'onde, suit la même ──┘   suit le pitch env transposé, on/off)
  pitch envelope transposée de `oscBTune`)
   │
   ▼
Enveloppe d'amplitude (attack / decay, + paramètre "punch" = transitoire de click ajouté)
   │
   ▼
Drive (waveshaper : soft clip tanh / hard clip / wavefolder) → filtre "tone" low-pass post-drive
   │
   ▼
Filtre multimode (SVF LP / HP / BP, cutoff + résonance) — sculpture créative post-drive
   │
   ▼
EQ 3 bandes entièrement paramétrique (freq / gain / Q réglables par bande)
   │
   ▼
Distorsion parallèle (tube / fuzz / bitcrush) — branche wet mixée au signal dry (knob Mix)
   │
   ▼
FX (chorus → reverb, tous deux avec dry/wet)
   │
   ▼
Compresseur (macro "Comp" pilotant threshold/ratio/makeup, + Attack séparé, release auto)
   │
   ▼
Clipper final (soft = tanh / hard = clip franc, avec Ceiling) — remplace le limiter de sécurité
   │
   ▼
Width (traitement mid/side : la largeur ne s'applique qu'au-dessus de ~150 Hz, le sub reste mono)
   │
   ▼
Gain de sortie → sortie stéréo
```

Détails d'implémentation :

- **Pitch envelope** : décroissance exponentielle `f(t) = f_end + (f_start − f_end) · exp(−t / τ)` avec τ dérivé de `sweepTime`. C'est LE paramètre qui fait le caractère du kick.
- **Osc B** : partage la pitch envelope de l'Osc A, transposée de `oscBTune` demi-tons (multiplication de fréquence par `2^(tune/12)`). Mixé avant l'enveloppe d'amplitude. Quand `oscBOn` est off, le code de l'Osc B ne doit pas tourner (branchement au niveau de la voix, pas par sample).
- **Filtre multimode** : `juce::dsp::StateVariableTPTFilter` (LP/HP/BP), 12 dB/oct. Résonance plafonnée pour rester stable (pas d'auto-oscillation en v1). Neutre par défaut : LP à 20 kHz.
- **EQ paramétrique** : 3 bandes peak (`juce::dsp::IIR::Coefficients::makePeakFilter`), chacune avec freq (20 Hz–18 kHz log), gain (±12 dB) et Q (0.3–6). Défauts : B1 = 60 Hz, B2 = 800 Hz, B3 = 6 kHz. Recalcul des coefficients seulement quand un paramètre change (pas à chaque bloc), avec smoothing sur le gain.
- **Punch** : mixe un court transitoire (burst de bruit filtré ou click de 1–3 ms) au tout début de la note. À 0 % → kick rond ; à 100 % → attaque très marquée.
- **Retrigger monophonique** : une nouvelle note coupe la précédente avec un fade de ~2 ms (éviter les clics). Pas de polyphonie.
- **Anti-aliasing** : PolyBLEP obligatoire sur carré/saw. Le sinus et le triangle n'en ont pas besoin (triangle : tolérable sans, à réévaluer à l'oreille).
- **Oversampling ×2 sur le DriveStage** (`juce::dsp::Oversampling`) — la distorsion forte crée de l'aliasing très audible sinon, surtout pour le hardstyle.
- Reverb : `juce::dsp::Reverb`. Chorus : `juce::dsp::Chorus`. Suffisants pour la v1.
- **Distorsion parallèle** : la branche wet traite une copie du signal (tube = waveshaper asymétrique doux, fuzz = gain élevé + hard clip + HPF léger, bitcrush = réduction de bit depth + sample rate). Compensation de gain approximative sur la branche wet pour que le knob Mix soit utilisable sans saut de volume.
- **Compresseur** : `juce::dsp::Compressor` en interne. Le knob macro "Comp" (0–100 %) mappe simultanément threshold (0 → −30 dB), ratio (1:1 → 8:1) et makeup gain compensatoire. Attack exposé (0.1–50 ms), release auto (~80 ms). Exposer la gain reduction courante à l'UI via un atomic pour le vu-mètre GR.
- **Clipper** : dernier étage non linéaire. Soft = tanh normalisé au ceiling, hard = clip franc. Pas d'oversampling en v1 (à réévaluer si aliasing audible en mode hard).
- **Width** : encodage mid/side avec crossover à 150 Hz (Linkwitz-Riley 2e ordre) — le side n'est modulé qu'au-dessus du crossover. 0 % = mono total, 100 % = stéréo native, jusqu'à 150 % = élargi. Comme le kick synthétisé est mono à la source, la stéréo provient du chorus et de la reverb ; le width agit sur cette composante.

---

## 5. Paramètres (APVTS)

Tous les IDs en camelCase, versionnés (`kickforge_params_v1`). Plages et défauts :

| ID | Nom UI | Plage | Défaut | Remarques |
|---|---|---|---|---|
| `genre` | Genre | choix ×5 | Hard techno | déclenche le chargement du preset |
| `waveform` | Osc A — forme | Sin/Tri/Sqr/Saw | Sin | |
| `pitchStart` | Pitch start | 50–2000 Hz (log) | 210 Hz | |
| `pitchEnd` | Pitch end | 25–200 Hz (log) | 48 Hz | |
| `sweepTime` | Sweep | 5–300 ms (log) | 42 ms | |
| `oscBOn` | Osc B on/off | on/off | off | |
| `oscBWave` | Osc B — forme | Sin/Tri/Sqr/Saw | Sqr | |
| `oscBTune` | Osc B — tune | −24 à +24 st | +12 st | relatif à l'Osc A |
| `oscBLevel` | Osc B — level | 0–100 % | 25 % | |
| `attack` | Attack | 0.1–20 ms | 1 ms | |
| `decay` | Decay | 50–2000 ms (log) | 340 ms | |
| `punch` | Punch | 0–100 % | 70 % | |
| `driveType` | Drive type | Soft/Hard/Fold | Hard | |
| `driveAmount` | Amount | 0–100 % | 65 % | mapping interne 1×–30× de gain |
| `driveTone` | Tone | 500 Hz–16 kHz (log) | 2.4 kHz | LPF post-drive |
| `filterType` | Filtre — type | LP/HP/BP | LP | |
| `filterCutoff` | Filtre — cutoff | 20 Hz–20 kHz (log) | 20 kHz | neutre par défaut |
| `filterReso` | Filtre — reso | 0–90 % | 20 % | |
| `eq{1,2,3}Freq` | Bande n — freq | 20 Hz–18 kHz (log) | 60 / 800 / 6000 Hz | |
| `eq{1,2,3}Gain` | Bande n — gain | ±12 dB | +3 / −2 / +1.5 dB | |
| `eq{1,2,3}Q` | Bande n — Q | 0.3–6 (log) | 0.8 | |
| `distType` | Dist type | Tube/Fuzz/Bit | Tube | |
| `distAmount` | Dist amount | 0–100 % | 20 % | intensité de la branche wet |
| `distMix` | Dist mix | 0–100 % | 40 % | dry/wet parallèle |
| `reverbMix` | Reverb | 0–100 % | 10 % | |
| `chorusMix` | Chorus | 0–100 % | 0 % | |
| `compAmount` | Comp | 0–100 % | 35 % | macro threshold+ratio+makeup |
| `compAttack` | Comp attack | 0.1–50 ms (log) | 8 ms | release auto |
| `clipType` | Clip type | Soft/Hard | Hard | |
| `clipCeiling` | Ceiling | −12 à 0 dB | −0.3 dB | |
| `width` | Width | 0–150 % | 100 % | mid/side, sub mono sous 150 Hz |
| `outputGain` | Gain | −24 à +6 dB | −1 dB | |
| `keyTrack` | Key track | on/off | off | la note MIDI transpose pitchEnd |

Tous les paramètres sont **automatisables** et **smoothés** (`juce::SmoothedValue`, ~20 ms) sauf les choix discrets.

---

## 6. Presets par genre

Charger un preset écrase les paramètres (sauf `outputGain`). Valeurs de départ à affiner à l'oreille :

| Paramètre | Techno | Hard techno | Hardstyle | EDM | Trance |
|---|---|---|---|---|---|
| waveform | Sin | Sin | Sin | Sin | Sin |
| oscB (on/forme/tune/level) | off | Sqr +12 / 15 % | Sqr +12 / 30 % | off | Tri 0 / 20 % |
| pitchStart | 180 Hz | 210 Hz | 320 Hz | 160 Hz | 150 Hz |
| pitchEnd | 50 Hz | 48 Hz | 55 Hz | 45 Hz | 50 Hz |
| sweepTime | 35 ms | 42 ms | 70 ms | 25 ms | 30 ms |
| attack | 1 ms | 1 ms | 0.5 ms | 1 ms | 2 ms |
| decay | 300 ms | 340 ms | 420 ms | 250 ms | 380 ms |
| punch | 55 % | 70 % | 85 % | 60 % | 40 % |
| driveType | Soft | Hard | Hard | Soft | Soft |
| driveAmount | 25 % | 65 % | 90 % | 10 % | 8 % |
| driveTone | 4 kHz | 2.4 kHz | 3.5 kHz | 8 kHz | 6 kHz |
| filtre (type/cutoff/reso) | LP 20 k / 20 % | LP 20 k / 20 % | LP 12 k / 30 % | LP 20 k / 10 % | LP 16 k / 15 % |
| eq gains B1/B2/B3 | +2/−1/0 | +3/−2/+1.5 | +2/0/+3 | +3/−3/+1 | +2/−1/+1 |
| distType / amount / mix | Tube 15/30 % | Bit 25/35 % | Fuzz 40/50 % | Tube 5/20 % | Tube 5/15 % |
| reverbMix | 8 % | 10 % | 5 % | 12 % | 20 % |
| chorusMix | 0 % | 0 % | 0 % | 5 % | 10 % |
| compAmount / attack | 30 % / 10 ms | 45 % / 8 ms | 55 % / 5 ms | 35 % / 12 ms | 25 % / 15 ms |
| clipType / ceiling | Soft / −0.5 | Hard / −0.3 | Hard / −0.1 | Soft / −1 | Soft / −1 |
| width | 60 % | 70 % | 80 % | 100 % | 110 % |

Note hardstyle : le caractère vient du sinus poussé très fort dans le hard clip — le résultat quasi carré et le mid-range saturé sont voulus. Si le rendu manque d'agressivité, augmenter `driveAmount` avant de toucher au reste.

**Bouton Random** : randomise chaque paramètre dans ±25 % de sa plage utile *autour du preset du genre actif* (pas sur toute la plage globale), pour rester musical.

---

## 7. UI

Maquette de référence fournie séparément (panneau sombre #17171b, sections #1e1e24, accent orange #d85a30). Dimensions : **760 × 480 px**, non redimensionnable en v1.

- **Header** : logo KickForge + barre de presets (5 boutons toggle, le genre actif en orange).
- **Waveform display** : rendu de la forme d'onde du kick généré (rendu offline dans un buffer à chaque changement de paramètre, throttlé à ~30 fps) + courbe de pitch en pointillés superposée avec annotation `f_start → f_end`.
- **Sections** (4 rangées) : Osc A (sélecteur d'onde + pitch start/end/sweep), Osc B (toggle on/off, sélecteur d'onde, tune, level), Enveloppe / Drive, Filtre (sélecteur LP-HP-BP + cutoff/reso), EQ paramétrique (courbe interactive : points draggables, bande sélectionnée via chips B1-B2-B3, affichage freq/gain/Q de la bande active) / Distorsion, FX, Compresseur (+ vu-mètre GR) / Clipper (+ mini-courbe de transfert), Sortie (play, Width M↔St, vu-mètre, gain, Random, Export). Prévoir ~700 px de hauteur (760 × 700).
- **Knobs** : `juce::Slider` rotatifs, LookAndFeel custom (cercle plat, indicateur trait coloré par section : orange = osc, vert = enveloppe, rouge = drive, violet = EQ, bleu = FX). Double-clic = retour au défaut. Valeur affichée sous le knob.
- **Bouton play** : déclenche le kick en interne (equiv. note-on C1) pour pré-écouter sans DAW.
- **Export WAV** : rendu offline du kick (durée = decay + queue de reverb, max 3 s), 48 kHz / 24 bits, via `FileChooser` async.

---

## 8. Plan de développement (ordre impératif)

Chaque étape doit compiler et être testable avant de passer à la suivante.

1. **Squelette** — CMake + JUCE fetch, plugin vide qui build en VST3 + Standalone, APVTS avec tous les paramètres déclarés, CI GitHub Actions Linux + Windows. *Critère : le Standalone s'ouvre, pluginval passe.*
2. **KickVoice minimal** — sinus + pitch env + amp env, déclenché au MIDI. *Critère : un kick techno reconnaissable sort du Standalone.*
3. **Formes d'onde + PolyBLEP + Osc B** — tri/carré/saw propres, couche B avec tune et level. *Critère : pas d'aliasing audible sur un saw à pitchStart max ; l'Osc B à +12 st enrichit sans détruire le sub.*
4. **DriveStage avec oversampling + FilterStage** — les 3 modes de drive + tone, puis le filtre multimode. *Critère : le preset hardstyle claque.*
5. **EQ paramétrique + distorsion parallèle + FX** — le cœur du sound design est complet.
6. **Compresseur + clipper + width + gain** — la fin de chaîne. *Critère : sub vérifié mono sous 150 Hz avec un analyseur de corrélation, GR meter fonctionnel.*
7. **Presets par genre + Random.**
8. **UI complète** — LookAndFeel, WaveformDisplay, câblage APVTS via attachments.
9. **Export WAV + bouton play.**
10. **Polish** — smoothing vérifié partout, retrigger sans clic, pluginval strictness 10, test dans FL Studio sur la machine cible.

---

## 9. Contraintes temps réel — non négociables

- **Aucune allocation, lock, ou I/O dans `processBlock`.** Tout est pré-alloué dans `prepareToPlay`.
- Pas de `std::string`, `new`, `malloc`, log, ni appel UI depuis le thread audio.
- Communication audio → UI uniquement par atomics ou FIFO lock-free (`juce::AbstractFifo`).
- Le rendu du WaveformDisplay se fait sur le message thread à partir d'une copie des paramètres, jamais en lisant l'état du thread audio directement.
- Gérer proprement les changements de sample rate et de taille de buffer (tout recalculer dans `prepareToPlay`).
- `processBlock` doit supporter les buffers de taille variable et les notes tombant au milieu d'un buffer (sample-accurate note-on).

---

## 10. Build et validation

- **Dev quotidien** : cible Standalone sous Linux, itération à l'oreille.
- **Validation** : `pluginval --strictness-level 10 --validate <plugin>` en CI sur les deux OS.
- **Livraison Windows** : build via GitHub Actions (windows-latest), artefact `.vst3` à déposer dans `C:\Program Files\Common Files\VST3\`.
- **Test final** : dans FL Studio, vérifier le chargement, l'automation des paramètres depuis le DAW, la sauvegarde/restauration de l'état dans le projet, et le comportement au changement de sample rate.
