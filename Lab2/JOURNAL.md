# JOURNAL.md — Journal d'équipe, CEG 3536, laboratoire 1 (automne 2026)

Équipe 2 : Rawda Massaoudou Tahirou et Alice Mpabuye — Section : A02 (Groupe 2) — Dépôt Git : https://github.com/ShotReady/CEG3536_equipeRawda-Alice

## Jalon J1 (validé dans Git au plus tard le 25 septembre 2026)

### Exigences de l'équipe
| Id | Exigence (reformulée par l'équipe) | Critère d'acceptation | Hypothèses |
|---|---|---|---|
| E1 | Initialisation | Après réinitialisation (NRST), l'état est ARRÊT (DEL rouge seule) dans les 100 ms[cite: 3]. | Horloge MSI de 4 MHz par défaut, pas de HAL[cite: 8]. |
| E2 | Défilement par User | Séquence ARRÊT → MARCHE_AVANT → ARRÊT → MARCHE_ARRIÈRE → ARRÊT via le bouton User[cite: 3, 4]. | Alternance gérée par la variable `.bss` `prochain_sens`. |
| E3 | Anti-rebond et détection de front | Fenêtre d'anti-rebond de 30 ms (`ANTIREBOND_MS`) ; un appui maintenu produit 1 seul événement[cite: 4, 8]. | Échantillonnage périodique à chaque pas de `fsm_step` (1 ms)[cite: 8]. |
| E4 | E-Stop par interruption | Appui sur PB2 déclenche l'interruption EXTI2 de priorité maximale ; l'ISR met l'état sûr instantanément[cite: 4, 8]. | Bouton E-Stop relié à PB2, niveau actif et front configurés dans `estop_init`[cite: 2, 8]. |
| E5 | Signalisation d'urgence | Clignotement de la DEL rouge à 2 Hz (250 ms allumée, 250 ms éteinte) en `ARRÊT_URGENCE`, appuis User ignorés[cite: 4]. | Pilotage par `clignote_compteur` et `clignote_phase` dans `fsm_maj_del`[cite: 36, 37]. |
| E6 | Acquittement | Passage d'urgence à ARRÊT uniquement par appui sur Touch En avec E-Stop relâché[cite: 4]. | Vérification simultanée de l'appui validé et du niveau brut de `BTN_ESTOP`[cite: 36]. |
| E7 | Touch En hors urgence | Bascule de la variable d'autorisation `touch_enabled` (0/1) et extinction brève (100 ms) de la DEL active[cite: 4]. | Géré par `touch_signal_compteur` dans la machine à états[cite: 36, 37]. |
| E8 | Temporisation | Routines basées sur une boucle calibrée (`delay_ms`) et base de temps de la FSM[cite: 4, 8]. | `DELAY_BOUCLES_PAR_MS = 1000` ajusté selon les mesures DWT[cite: 8]. |
| E9 | Robustesse | Cohérence du système face aux appuis simultanés ou aux transitoires d'urgence[cite: 4]. | Un seul point d'appel pour `led_set` via `fsm_maj_del`[cite: 8, 37]. |

### Rôles et rotation
| Séance | Réalise | Valide (essais, mesures, relecture) |
|---|---|---|
| Séance 0 | Rawda Massaoudou Tahirou | Alice Mpabuye |
| Séance 1 & Démonstration (22 sept.) | Alice Mpabuye | Rawda Massaoudou Tahirou |

### Échéancier des laboratoires 1 à 5
| Laboratoire | Séances | Démonstration | Remise | Responsable du suivi |
|---|---|---|---|---|
| 1 | Septembre 2026 | 22 septembre 2026 | 9 octobre 2026 | Rawda & Alice |
| 2 | Octobre 2026 | Octobre 2026 | Octobre 2026 | Rawda & Alice |
| 3 | Novembre 2026 | Novembre 2026 | Novembre 2026 | Rawda & Alice |
| 4 | Novembre 2026 | Décembre 2026 | Décembre 2026 | Rawda & Alice |
| 5 | Décembre 2026 | Décembre 2026 | Décembre 2026 | Rawda & Alice |

## Journal des séances

### Séance 0 — 15 septembre 2026 — réalise : Rawda Massaoudou Tahirou / valide : Alice Mpabuye
- **Objectifs** : Prise en main de la plateforme Leafy et de la carte Nucleo-L552ZE-Q, configuration de STM32CubeIDE, initialisation du dépôt Git d'équipe[cite: 9, 41].
- **Fait** : Importation du projet de démarrage vide, compilation sans avertissement et test d'affichage de la DEL rouge seule[cite: 9, 41].
- **Décisions** : Adoption de l'organisation des fichiers source dans `Core/Src` et utilisation du fichier `registres.inc`[cite: 6, 7].
- **Difficultés et solutions** : Repérage de la particularité de la famille STM32L5 (le registre de sélection de port `EXTI_EXTICR1` se situe dans le bloc `EXTI` à l'offset `0x60`, et non dans `SYSCFG`)[cite: 7, 41].
- **Essais et mesures** : Vérification du bon état initial (E1)[cite: 9].
- **Validations Git** : Commit initial de la structure du projet avec validation de chaque membre[cite: 9].

### Séance 1 & Démonstration — 22 septembre 2026 — réalise : Alice Mpabuye / valide : Rawda Massaoudou Tahirou
- **Objectifs** : Implémentation des modules `buttons.s`, `estop.s` et `fsm.s`, validation des exigences E1 à E9, et démonstration officielle devant l'assistant en laboratoire[cite: 5, 9].
- **Fait** : 
  - Écriture de la logique d'anti-rebond par échantillons consécutifs (30 ms) dans `button_pressed`.
  - Configuration de l'interruption externe EXTI2 sur PB2 avec priorité maximale (`NVIC_PRIO_MAX`) et mise en place de l'état sûr immédiat par écriture atomique dans `BSRR` dans l'ISR `EXTI2_IRQHandler`.
  - Réalisation de la machine à états complète (`fsm_step`, `fsm_maj_del`) gérant l'alternance des sens de marche, le clignotement de la DEL rouge à 2 Hz en urgence, l'acquittement sécurisé par Touch En et l'extinction brève (100 ms).
- **Décisions** : Traitement de la sécurité critique (< 10 ms) directement dans l'ISR pour couper immédiatement les DEL verte et bleue même au sein d'une temporisation[cite: 4, 8].
- **Difficultés et solutions** : Ajustement de la logique XOR dans `button_raw` pour normaliser l'état actif et synchronisation du compteur de cycles `DWT_CYCCNT` pour mesurer précisément la période de clignotement (T6)[cite: 10, 30].
- **Essais et mesures** : Réussite et validation de tous les essais T1 à T10 (test de robustesse, appuis simultanés, anti-rebond, non-régression)[cite: 9, 10]. Démonstration validée avec succès par l'assistant en fin de séance[cite: 9].
- **Validations Git** : Validation et synchronisation finale de tous les codes sources assembleur sur le dépôt distant de l'équipe[cite: 9].

## Tableau des essais (T1 à T10)
| Essai | Date | Résultat observé | Verdict | Preuve (fichier) |
|---|---|---|---|---|
| T1 Réinitialisation | 22 sept. 2026 | DEL rouge seule allumée dans les 100 ms après un reset | Conforme[cite: 9] | Capture IDR/ODR[cite: 9] |
| T2 Cycle User | 22 sept. 2026 | Séquence exacte : Rouge → Verte → Rouge → Bleue → Rouge | Conforme[cite: 9, 12] | Relevé des états ODR[cite: 9] |
| T3 Anti-rebond | 22 sept. 2026 | 20 appuis validés donnent 20 transitions exactes ; 1 appui maintenu de 2 s génère 1 seule transition | Conforme[cite: 9, 13] | Watch sur `compteur_transitions`[cite: 9, 10] |
| T4 Niveaux logiques | 22 sept. 2026 | Niveaux actifs et résistances de rappel (`PUPDR`) vérifiés par lecture directe de `IDR` | Conforme[cite: 9, 13] | Tableau des mesures IDR[cite: 10] |
| T5 E-Stop | 22 sept. 2026 | Passage immédiat en urgence et extinction de la DEL active en < 10 ms | Conforme[cite: 10, 13] | Capture oscilloscope (PB2 et PC7)[cite: 10] |
| T6 Clignotement | 22 sept. 2026 | Période mesurée de la DEL rouge en urgence égale à 500 ms ± 10% | Conforme[cite: 10, 14] | Watch sur `blink_cycles`[cite: 10] |
| T7 Acquittement | 22 sept. 2026 | Retour en ARRÊT si Touch En appuyé avec E-Stop relâché ; refus si maintenu | Conforme[cite: 10, 14] | Suivi de la variable `etat`[cite: 10] |
| T8 User ignoré en urgence | 22 sept. 2026 | Les appuis sur le bouton User n'ont aucun effet pendant l'état `ARRÊT_URGENCE` | Conforme[cite: 10, 14] | Relevé de l'état FSM[cite: 10] |
| T9 Touch En hors urgence | 22 sept. 2026 | `touch_enabled` alterne entre 0 et 1 avec extinction brève (100 ms) de la DEL active | Conforme[cite: 10, 14] | Watch sur `touch_enabled`[cite: 10] |
| T10 Robustesse | 22 sept. 2026 | Appuis simultanés et E-Stop déclenché pendant une transition traités sans incohérence | Conforme[cite: 10, 14] | Rapport de test et traces débogueur[cite: 10] |

## Routine conservée pour L3-A
- **Routine** : `button_pressed` (implémentée dans `buttons.s`)
- **Interface** : Reçoit l'identifiant du bouton dans `r0` (`0` pour User, `1` pour E-Stop, `2` pour Touch En) et retourne `1` si un front d'appui valide est détecté, `0` sinon[cite: 5, 8]. Respecte entièrement les conventions AAPCS avec sauvegarde de `r4-r5-r6-lr`[cite: 8].
- **Cas d'essai** : Soumission d'une suite d'impulsions bruitées (rebonds simulés de < 30 ms) pour vérifier que le filtre ignore les instabilités et ne déclenche qu'un seul front validé après le seuil temporel. Cette routine sera réécrite en langage C au Laboratoire 3 pour comparaison de performance[cite: 3, 6].

## Déclaration des sources et de l'usage d'outils d'IA générative
- **Sources** : Manuel de référence STM32L5 (RM0438), Guide de la plateforme Leafy, Notes de cours 
- **Outils d'IA** : Aucun usage d'outil d'IA générative 