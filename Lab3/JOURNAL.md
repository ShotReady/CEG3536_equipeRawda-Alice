# JOURNAL.md — Journal d'équipe, CEG 3536, laboratoire 3 (automne 2026)

Équipe : `Rawda Massaoudou Tahirou` et `Alice Mpabuye` — Section : `A02` — Dépôt Git : `https://github.com/ShotReady/CEG3536_equipeRawda-Alice.git`

## Jalon J2 — module d'événements testé sur hôte

| Plateforme    | Compilateur et version | Commande            | Résultat (cas réussis / échecs)                    | Date et validation Git                                                           |
| ------------- | ---------------------- | ------------------- | -------------------------------------------------- | -------------------------------------------------------------------------------- |
| Windows       | gcc.exe (MinGW-W64) 13.2.0 | `make` dans `hote/` | 9 cas réussis / 0 échec | 17 nov. 2026; commit 4291823 |
| Linux         |                        |                     |                                                    |                                                                                  |
| STM32 (cible) | arm-none-eabi-gcc 14.3.1 (GNU Tools for STM32 14.3.rel1) | commande série `q` | 9 cas réussis / 0 échec (`echecs = 0`, figure A10) | 10 nov. 2026; commit ff5fb18 (« code lab3 », 7 nov. 2026), fusionné dans a6ce9b1 |

Cas ajoutés par l'équipe (au-delà des 9 fournis) :

| Cas | Description                                                    | Résultat |
| --- | -------------------------------------------------------------- | -------- |
| 10  | Aucun cas supplémentaire consigné dans le rapport (T7 : 9 cas) | ---      |

Différences observées entre hôtes (tailles des types, options, avertissements) :

## Rôles et rotation

| Séance             | Réalise                                | Valide   | Module C écrit par chacun                              | Routine assembleur écrite ou modifiée par chacun                 |
| ------------------ | -------------------------------------- | -------- | ------------------------------------------------------ | ---------------------------------------------------------------- |
| Séance 1 (10 nov.) | Rawda et Alice, en même temps          | Les deux | Programmation en paire : chacune a écrit des modules C | Programmation en paire : chacune a écrit des routines assembleur |
| Période d'étude    | Rawda et Alice (préparation, en paire) | Les deux | idem                                                   | Idem                                                             |
| Séance 2           | Sans objet : laboratoire réalisé en une seule séance (10 nov.) | --- | --- | --- |

Rédaction : Rawda a écrit le rapport; Alice a tenu le journal. Rawda a effectué les commits Git pour les deux.

## Calculs préalables

| Grandeur                        | Formule                                         | Valeur                                     | Vérifiée par |
| ------------------------------- | ----------------------------------------------- | ------------------------------------------ | ------------ |
| `RPM_Q16_K`                     | round(240 × 65 536 / 748)                       | 21 027,59 → **21 028**                     | T3           |
| Erreur relative de la constante | (k − 240 × 65 536 / 748) / (240 × 65 536 / 748) | ≈ 0,002 % (limite : 0,05 %)                | T3           |
| fronts maximal sans débordement | 2³² / k                                         | **204 249** (donc OK pour fronts ≤ 65 535) | T3           |
| `LPUART1_BRR`                   | 256 × 4 000 000 / 115 200                       | 8 888,9 → **8 889**                        | T9           |
| Débit ADC par voie              | 4 MHz / (2 × (640,5 + 12,5))                    | ≈ **3 063 Hz**                             | T4           |
| Durée d'un tour du tampon DMA   | 16 / débit par voie                             | ≈ **5,2 ms** (≈ 191 tours/s)               | T4           |
| Résolution de la température    | 3 300 mV / 4 095 / 10,9 mV/°C                   | ≈ **0,07 °C** par pas (≈ 0,81 mV)          | T6           |

## Étalonnage de la température (E6)

| Mesure                                           | Température de référence (°C) | `temp_mv` lu | `TEMP_MV_REF` / `TEMP_C10_REF` retenus | Position de S2 |
| ------------------------------------------------ | ----------------------------- | ------------ | -------------------------------------- | -------------- |
| Ambiante                                         | ---                           | 1 816 mV     | 1 816 mV / 263 (26,3 °C)               | Gauche         |
| Capteur tenu entre deux doigts (30 s)            |                               |              |                                        |                |
| Conditionnement externe (pont OUT–IN ou circuit) | ---                           | Lecture identique à l'ambiante (± 1 °C, T6) | ---             | Droite         |

Analyse interne / externe (plage, résolution, bruit) :
- **Interne** : 1 pas ≈ 0,81 mV ≈ 0,07 °C. Sur la plage utile de 15 à 40 °C, la sortie varie d’environ 272 mV, soit environ 340 pas.
- Externe (Gain G) : la résolution devient ≈ 0,07 / G °C par pas et l’excursion passe à 272 × G mV, qui doit rester sous 3 300 mV (G < ≈ 12).
- Bruit : l'amplification (gain G) amplifie aussi le bruit du capteur et de l'amplificateur; la moyenne sur 64 échantillons ne réduit que la part aléatoire (facteur 8).
- L'étalonnage corrige seulement le décalage; la pente de −10,9 mV/°C est celle de la fiche technique.

## Journal des séances

### Séance 1 — `10 novembre 2026` — réalise : `Rawda et Alice` / valide : `les deux`
- Objectifs : réaliser et démontrer le laboratoire 3 en entier (T1 à T10).
- Fait : application C (main, fsm, acq, evq, telem, adc_dma), routine `enc_fronts_vers_rpm_q16`, banc `bench_l3a`, désassemblages. Les 10 essais ont été démontrés à l’assistant le 10 novembre et sont conformes.
- Décisions (découpage des modules, format des trames, routine L3-A choisie) :
	- Découpage en couches : bas niveau assembleur, acquisition, commande, communication, orchestration.
	- Organisation B (file d’événements) retenue plutôt que la scrutation.
	- Trame CSV `T,ticks_ms,etat,sens,consigne_pct,pwm_pct,temp_c_x10,vitesse_rpm_x100,nb_evenements,evq_debordements`, précédée d’une ligne d’en-tête `H`.
	- Routine L3-A choisie : `motor_set_pwm`.
- Difficultés et solutions :
	- alimentation du port G (`PWR_CR2.IOSV = 1`) pour PG7/PG8;
	- requête DMAMUX n° 5 confirmée par l’avance de `tours_dma`.
- Mesures :
	- Température ambiante : `temp_mv` = 1 816 mV, soit 263 (26,3 °C).
	- `tours_dma` ≈ 191 tours/s (342 901 à ≈ 1 791 s).
	- Vitesse à pleine consigne ≈ 357 à 359 tr/min.
	- 40 événements produits pour 32 places : débordements de 0 à 8.
	- Cycles DWT à -O0 : asm 31 à 32, C 50.
- Validations Git : commit « code lab3 » (ff5fb18, 7 nov. 2026), fusionné dans a6ce9b1.

### Période d'étude — travail sur hôte
- Fait (evq, test_evq, cas ajoutés, L3-B / L3-C rédigés) : le travail sur le laboratoire a commencé avant le 10 novembre, en préparation, en programmation en paire. `evq.c` / `evq_test.c` compilés sur hôte avec le `Makefile` de `hote/` (9 cas fournis, aucun cas ajouté); conception L3-B (organisation A vs B) et objet d'acquisition L3-C rédigés. Après la démonstration du 10 nov., ajout par Rawda, par initiative personnelle, du dossier `trace_l3d/` et du script `l3d_trace.sh` (fichiers datés du 15 nov. 2026) pour régénérer les désassemblages -O0 / -O2 et les tailles; ces fichiers ne faisaient pas partie de la démonstration.
- Validations Git : premier commit « code lab3 » (ff5fb18, 7 nov. 2026); commit 4291823 (17 nov. 2026).

### Séance 2
Sans objet : le laboratoire a été réalisé en une seule séance, le 10 novembre 2026 (voir séance 1).

## Composante L3-A : comparaison assembleur / C

Routine comparée : `motor_set_pwm`

| Cas | Entrée     | Résultat assembleur | Résultat C | Effet de bord (registre / variable) | Écart |
| --- | ---------- | ------------------- | ---------- | ----------------------------------- | ----- |
| 1   | 0          | 0                   | 0          | TIM1->CCR1 = 0                      | 0     |
| 2   | 1          | 2                   | 2          | TIM1->CCR1 = 2                      | 0     |
| 3   | 25         | 50                  | 50         | TIM1->CCR1 = 50                     | 0     |
| 4   | 50         | 100                 | 100        | TIM1->CCR1 = 100                    | 0     |
| 5   | 75         | 150                 | 150        | TIM1->CCR1 = 150                    | 0     |
| 6   | 99         | 198                 | 198        | TIM1->CCR1 = 198                    | 0     |
| 7   | 100        | 200                 | 200        | TIM1->CCR1 = 200                    | 0     |
| 8   | 0xFFFFFFFF | 200(saturation)     | 200        | TIM1->CCR1 = 200                    | 0     |

| Version    | Cycles DWT (moyenne) | Taille (octets) | Registres utilisés | Observations (prologue, volatile)                                                                                                            |
| ---------- | -------------------- | --------------- | ------------------ | -------------------------------------------------------------------------------------------------------------------------------------------- |
| Assembleur | 31 à 32              | 24              | R0, R1, R2         | Feuille, sans pile; `mul.w` + `udiv`; saturation par `it hi`                                                                                 |
| C, -O0     | 50                   | 64              | R0, R2, R3, R7     | Prologue `push {r7}` / `sub sp, #20`, cadre de 24 octets; `pct` et `ccr1` sur la pile; division par 100 via `umull` (0x51EB851F) + `lsrs #5` |
| C, -O2     | ---                  | 20              | R0, R3             | Ni prologue ni épilogue; (pct × 200)/100 devient `lsls r0, r0, #1`; l’écriture volatile `str r0, [r3, #52]` est conservée                    |

## Composante L3-C : objet d'acquisition

- Version 1 (validation Git) : moyenne et relation linéaire seulement; n'a pas fait l'objet d'une validation Git distincte.
- Version 2 (validation Git) et ce qui a changé : commit « code lab3 » (ff5fb18, 7 nov. 2026). Ajout de `acq_config_filtre` et, dans `acq_valeur`, de la zone morte de 3 % et de l’hystérésis de 2 points. Température lissée sur 64 échantillons. Le code appelant change seulement à l’initialisation, et `ADC_N_MOY` (taille du tampon DMA) a été augmenté.
- Essai d'indépendance des deux instances : aux figures A3 à A5, la tension du potentiomètre passe de 0 mV à la pleine échelle alors que `temp_mv` reste ≈ 1 816 mV (`temp_c_x10` = 263 ± 1).
- Comparaison avec la version impérative (annexe) : avec des variables globales, il faut des variables ou tableaux par voie, le filtrage est dupliqué, et un changement se répercute dans `main.c`, `fsm.c` et tous les lecteurs, avec un risque d’incohérence. Avec l’objet, le changement tient dans `acq.c` plus un appel d’init, et une troisième voie n’est qu’une instance de plus.
- Limites : ≈ 50 octets par instance, plus 2 octets par échantillon et par voie (64 o à N = 16, 256 o à N = 64); pas de polymorphisme (conversion non linéaire = pointeur de fonction); DMA qui écrit pendant la lecture (moyenne pouvant mélanger deux tours); `n_moy` ≤ nombre d’échantillons par voie.

## Composante L3-D : traces

| Élément                                              | Preuve (fichier)                                                                                                       |
| ---------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------- |
| Désassemblage -O0 annoté                             | Annexe B, tableau B1; figure A16 (débogueur). Complément ajouté le 15 nov. : `trace_l3d/` (script `l3d_trace.sh`)      |
| Désassemblage -O2 annoté                             | Annexe B, tableau B1; complément : `trace_l3d/`                                                                        |
| Trois instructions et leur encodage Thumb            | Section 6 et annexe B : `movs r2,#200` (0x22C8), `mul.w r3,r2,r3` (0xFB02 0xF303), `umull r2,r3,r2,r3` (0xFBA2 0x2303) |
| Captures du débogueur (R0–R3, SP, variable volatile) | Figure A2 (SP = 0x20030000, LR = 0x08001B73); figure A16 et annexe B (relectures de `etat`/`pwm_pct`)                  |
| Cycles DWT -O0 / -O2 et transformation analysée      | Figures A14 et A15 (-O0 seulement); transformation : `(pct × 200)/100` → `lsls r0, r0, #1` à -O2                       |

## Tableau des essais (T1 à T10)

| Essai                     | Date         | Résultat observé                                                                                                                              | Verdict  | Preuve (fichier)                   |
| ------------------------- | ------------ | --------------------------------------------------------------------------------------------------------------------------------------------- | -------- | ---------------------------------- |
| T1 Initialisation en C    | 10 nov. 2026 | ARRÊT, rouge seule, moteur immobile; ligne H reçue; première trame dans les 300 ms                                                            | Conforme | Figure A1                          |
| T2 Appel C → assembleur   | 10 nov. 2026 | R0 paramètre et retour, SP multiple de 8, LR valide                                                                                           | Conforme | Figure A2                          |
| T3 Virgule fixe           | 10 nov. 2026 | 5 valeurs de test (0, 1, 748, 1 870, 65 535 fronts) : erreur ≈ 0,002 %, sous la limite de 0,05 %; ex. 748 fronts → 15 728 944 (240,00 tr/min) | Conforme | Tableau de la section 5 du rapport |
| T4 ADC et DMA             | 10 nov. 2026 | Brut ≈ 0, ≈ 2 048, ≈ 4 095 (± 2 %); débit conforme au calcul ± 10 %                                                                           | Conforme | Figures A3 à A5                    |
| T5 Consigne               | 10 nov. 2026 | Suit 0 à 100 % avec rampe; sans effet en ARRÊT; pas d’oscillation au repos                                                                    | Conforme | Figures A6 à A8                    |
| T6 Température            | 10 nov. 2026 | Écart ≤ 2 °C au repos; hausse de plusieurs degrés; même lecture ± 1 °C avec le pont externe                                                   | Conforme | Démonstration devant l'assistant   |
| T7 File d'événements      | 10 nov. 2026 | autotest de la file réussi (9 cas); débordements comptés et signalés                                                                          | Conforme | Figures A9 et A10                  |
| T8 E-Stop                 | 10 nov. 2026 | PWM et STB coupés en moins de 1 ms; EV_ESTOP consigné; trames intactes; aucune reprise sans Touch En                                          | Conforme | Figures A11 et A12                 |
| T9 Télémétrie             | 10 nov. 2026 | Format valide, 150 trames ± 10 % en 30 s, aucune trame tronquée                                                                               | Conforme | Figure A13                         |
| T10 Comparaison et traces | 10 nov. 2026 | Zéro écart; cycles et tailles consignés; trois instructions reliées à leur encodage                                                           | Conforme | Figures A14 à A16 ; annexe B       |

## Déclaration des sources et de l'usage d'outils d'IA générative
- Sources (complément, corrigé du laboratoire 2 le cas échéant, CMSIS, manuel) : énoncé du laboratoire 3 (v1.3); complément `CEG3536_Lab3_Complement.zip` (Brightspace : en-têtes CMSIS, squelettes des modules, `test_evq.c`, `telemetrie.py`, `JOURNAL.md`); projet du laboratoire 2 de l'équipe; manuel de référence RM0438, fiche technique DS12737, PM0264; guide de la plateforme Leafy; manuel Zhu, 4e éd.; fichiers de démarrage générés par STM32CubeIDE; CMSIS.
- Outils d'IA : Claude Sonnet 5.5 (Anthropic) pour certaines sections du journal
