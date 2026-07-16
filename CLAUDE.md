# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Le projet

KickForge : synthétiseur de kick VST3 + Standalone (JUCE 8, CMake pur, C++20).
Moteur v2 à 3 couches (attack / punch / crunch). Dev sous Linux/Fedora, cible
finale Windows (FL Studio). Open source AGPLv3, repo `Bebop-Tech/kickforge`.

**Documents de référence — à lire avant toute modification de comportement :**
- `BRIEF-KICKFORGE.md` — spec v1 (chaîne DSP, paramètres, presets, contraintes)
- `BRIEF-KICKFORGE-V2.md` — spec v2 (architecture 3 couches, routing reverb, migration)
- `docs/ARCHITECTURE.md` — carte détaillée du code, patterns établis, pièges connus

## Règles de travail imposées par Benjamin

- **Les briefs sont la source de vérité.** Aucune fonctionnalité hors brief sans
  demande explicite ; sinon la noter dans `BACKLOG.md` et continuer.
- **Tout choix non couvert par les briefs se propose AVANT implémentation**
  (AskUserQuestion), avec une recommandation.
- **TDD strict pour le DSP** : test qui échoue d'abord, puis implémentation.
- Chaque étape doit compiler, passer `ctest` ET pluginval strictness 10 avant la suivante.
- Jamais de commit/push sans demande explicite. Identité git locale : Bebop Tech.
  Plusieurs comptes `gh` peuvent coexister sur la machine — vérifier que
  **Bebop-Tech est le compte actif** (`gh auth status`) avant toute opération réseau.

## Commandes

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # configure (fetch JUCE via CPM)
cmake --build build --parallel                        # build tout (VST3, Standalone, tests)
ctest --test-dir build --output-on-failure            # tous les tests
./build/dsp_tests_artefacts/Release/dsp_tests '[KickEngine]'   # un tag Catch2
./build/dsp_tests_artefacts/Release/dsp_tests 'nom du test*'   # un test précis
build/KickForge_artefacts/Release/Standalone/KickForge         # lancer le Standalone
<pluginval> --strictness-level 10 --validate build/KickForge_artefacts/Release/VST3/KickForge.vst3
```

pluginval : binaire à télécharger dans le scratchpad
(`https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Linux.zip`).
Le binaire de test est `build/dsp_tests_artefacts/Release/dsp_tests` (console app
JUCE) — PAS `build/dsp_tests` (un vieux binaire peut traîner là après un renommage
de cible).

## Architecture en deux phrases

`KickEngine` (src/dsp) orchestre les 3 couches et rend **deux bus mono** :
`busAP` (attack + punch drivé) qui alimente le send de reverb, et `mix` (+ crunch,
qui reste sec) qui traverse la chaîne commune filtre → EQ → dist parallèle →
chorus → (+ wet reverb) → comp → clip → width → gain. `kickforge_dsp` (KickVoice,
AttackLayer) est du C++ pur sans JUCE ; tout le reste des étages dépend de
`juce_dsp` et est compilé à la fois dans le plugin et dans `dsp_tests`
(`KICKFORGE_JUCE_DSP_SOURCES` dans CMakeLists.txt).

Détails, threading, patterns et pièges : **`docs/ARCHITECTURE.md`** — le lire
avant de toucher au DSP, aux paramètres ou à l'UI.

## Les 3 pièges qui ont réellement mordu (résumé — détails dans ARCHITECTURE)

1. **Ordre `setParameters()` puis `reset()`** : les `reset()` JUCE snappent les
   smoothers sur la cible courante. L'inverser = rampe fantôme depuis les défauts
   (gain de drive 9× pendant 20 ms, mix chorus 0.5...).
2. **Toute extinction de source doit mourir AVANT sa non-linéarité** (voir
   `beginQuickRelease`) — un fade post-drive laisse la discontinuité traverser
   l'oversampler à gain plein.
3. **Portabilité MSVC** : `#include <algorithm>` explicite (GCC l'importe via
   `<cmath>`, pas MSVC), jamais `M_PI`, et les littéraux UTF-8 destinés à
   `juce::String` passent par `juce::String::fromUTF8`.
