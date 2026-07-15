# KickForge v2 — Spec « kick en 3 couches »

Extension du brief v1 (`BRIEF-KICKFORGE.md`), qui reste valable pour tout ce qui n'est pas
contredit ici. Origine : retour utilisateur — pouvoir sculpter séparément l'**attack**
(click initial), le **punch** (corps tonal) et le **crunch** (queue saturée), avec une
distorsion dédiée au crunch et une reverb qui n'affecte que attack + punch.

---

## 1. Architecture — signal flow v2

```
MIDI note-on ── déclenche les 3 couches en parallèle
   │
   ├─ ATTACK : burst de bruit filtré (LP), enveloppe de décroissance courte
   │           → atkLevel
   ├─ PUNCH  : Osc A/B + pitch envelope + amp env (le cœur v1)
   │           → drive du punch (l'actuel DriveStage : type/amount/tone)
   │           → punchLevel
   └─ CRUNCH : oscillateur dédié accordé sur pitchEnd (+ crunchTune)
               → enveloppe attack lent / long decay
               → SA distorsion dédiée (type/amount/tone, oversamplée ×2)
               → crunchLevel
   │
   ├── busAP = attack + punch ──────────────┐
   ├── mix = busAP + crunch                 │ (send mono, pré-chaîne commune)
   ▼                                        ▼
 filtre multimode → EQ 3 bandes         REVERB (wet only, stéréo)
 → distorsion parallèle → chorus            │
   │                                        │
   └────────────── somme stéréo ◄───────────┘
                        │
              comp → clip → width → gain → sortie
```

- **Reverb = send alimenté par busAP uniquement** (le crunch reste sec). Le wet
  stéréo est réinjecté avant le compresseur : comp/clip/width traitent aussi la queue
  de reverb (le width en a besoin, c'est sa source stéréo principale).
- **Le chorus reste global** (sur le mix complet).
- La **distorsion parallèle commune est conservée** ; celle du crunch s'y ajoute en amont.
- Le drive v1 devient officiellement « drive du punch » (IDs de paramètres inchangés).

## 2. Les couches

### Attack
Généralisation du `punch` v1 : burst de bruit blanc déterministe → low-pass `atkTone`
→ décroissance exponentielle `atkDecay`. Indépendant de l'attack de l'enveloppe du punch.

### Punch
Identique au cœur v1 : Osc A (+ Osc B), pitch envelope exponentielle, amp env
attack/decay, drive dédié. Seul ajout : `punchLevel`.

### Crunch
Oscillateur dédié (Sin/Tri) dont la fréquence est **fixe = pitchEnd × 2^(crunchTune/12)**
(pas de sweep : c'est la queue, elle est déjà posée). Enveloppe attack lent + long decay.
Poussé dans sa propre distorsion (réutilise `DriveStage` : soft/hard/fold + tone,
oversampling ×2). Le caractère hardstyle vient de cette couche saturée qui tient.

## 3. Paramètres (APVTS v2 — `kickforge_params_v2`)

### Nouveaux

| ID | Nom UI | Plage | Défaut | Remarques |
|---|---|---|---|---|
| `atkLevel` | Attack — level | 0–100 % | 70 % | hérite de l'ancien `punch` |
| `atkDecay` | Attack — decay | 1–30 ms (log) | 3 ms | |
| `atkTone` | Attack — tone | 500 Hz–16 kHz (log) | 5 kHz | LP sur le bruit |
| `punchLevel` | Punch — level | 0–100 % | 100 % | |
| `crunchLevel` | Crunch — level | 0–100 % | 0 % | 0 = comportement v1 |
| `crunchWave` | Crunch — forme | Sin/Tri | Sin | |
| `crunchTune` | Crunch — tune | −12–+12 st | 0 st | relatif à pitchEnd |
| `crunchAttack` | Crunch — attack | 5–200 ms (log) | 30 ms | monte après le punch |
| `crunchDecay` | Crunch — decay | 100–2000 ms (log) | 500 ms | |
| `crunchDriveType` | Crunch — drive | Soft/Hard/Fold | Hard | |
| `crunchDriveAmount` | Crunch — amount | 0–100 % | 60 % | gain 1×–30× |
| `crunchDriveTone` | Crunch — tone | 500 Hz–16 kHz (log) | 3 kHz | LPF post-drive |

### Modifiés / supprimés

- `punch` (v1) **supprimé** → migré vers `atkLevel`.
- Tous les autres paramètres v1 inchangés (IDs et plages).

## 4. Migration d'état v1 → v2

`setStateInformation` accepte les deux tags :
- état `kickforge_params_v2` → chargement direct ;
- état `kickforge_params_v1` → mapping : `punch` → `atkLevel`, nouveaux paramètres aux
  défauts ci-dessus (`crunchLevel` 0 % ⇒ un projet v1 rechargé sonne quasi identique —
  seule différence assumée : la reverb v2 ne mouille plus le crunch, inaudible à 0 %).

## 5. Presets par genre v2 (provisoires, à affiner à l'oreille)

| Paramètre | Techno | Hard techno | Hardstyle | EDM | Trance |
|---|---|---|---|---|---|
| atkLevel | 55 % | 70 % | 85 % | 60 % | 40 % |
| atkDecay | 2 ms | 3 ms | 4 ms | 2 ms | 2 ms |
| atkTone | 4 kHz | 5 kHz | 6 kHz | 5 kHz | 4 kHz |
| punchLevel | 100 % | 100 % | 100 % | 100 % | 100 % |
| crunchLevel | 25 % | 45 % | 80 % | 10 % | 15 % |
| crunchWave | Sin | Sin | Sin | Sin | Sin |
| crunchTune | 0 | 0 | 0 | 0 | 0 |
| crunchAttack | 40 ms | 30 ms | 20 ms | 50 ms | 60 ms |
| crunchDecay | 400 ms | 500 ms | 700 ms | 300 ms | 400 ms |
| crunchDrive (type/amount/tone) | Soft 40 / 2.5 k | Hard 65 / 3 k | Hard 95 / 3.5 k | Soft 25 / 4 k | Soft 20 / 3 k |

Les valeurs v1 des autres paramètres restent le point de départ ; le `driveAmount` du
punch pourra baisser sur Hardstyle (une partie de l'agressivité migre vers le crunch).

## 6. UI v2

- Fenêtre **760 × 841** (une rangée de plus), toujours non redimensionnable.
- Nouvelle rangée « couches » insérée sous la waveform :
  `ATTACK (level/decay/tone) | PUNCH (level, renvoi visuel vers Osc/Env) | CRUNCH (level, wave, tune, attack, decay + drive type/amount/tone)`
- La section Drive existante est retitrée « DRIVE (punch) ».
- Couleurs : attack = jaune comp, crunch = rouge drive (les couches saturées partagent le code couleur rouge).
- Waveform display et export : inchangés dans le principe (rendu de la chaîne complète v2).

## 7. Plan de développement v2 (ordre impératif, mêmes règles que v1)

1. **AttackLayer** — extraction du transitoire v1 en couche autonome (TDD). *Critère : à réglages équivalents, sortie identique au punch v1.*
2. **CrunchLayer** — osc dédié + enveloppe + drive propre (TDD). *Critère : queue saturée qui tient, accordée sur pitchEnd.*
3. **KickVoice v2** — orchestration 3 couches, bus AP/mix (TDD). *Critère : crunchLevel 0 ⇒ bit-identique à la v1 (hors reverb).*
4. **Routing reverb send** — processeur + WavExporter + WaveformDisplay. *Critère : crunch sec vérifié par mesure.*
5. **APVTS v2 + migration v1.** *Critère : un état v1 sauvegardé se recharge sans perte.*
6. **Presets v2.** *Critère : le hardstyle claque plus qu'en v1.*
7. **UI v2 (760 × 820).**
8. **Validation** — pluginval strictness 10, écoutes, retour du frère.

## 8. Contraintes

Les contraintes temps réel de la section 9 du brief v1 s'appliquent intégralement.
Chaque étape compile, passe les tests et pluginval avant la suivante.
