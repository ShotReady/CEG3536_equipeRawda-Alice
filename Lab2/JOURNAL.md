# JOURNAL.md — Journal d'équipe, CEG 3536, laboratoire 1 (automne 2026)

Équipe 2 : Rawda Massaoudou Tahirou et Alice Mpabuye — Section : A02 (Groupe 2) — Dépôt Git : https://github.com/ShotReady/CEG3536_equipeRawda-Alice

## Jalon J1 (validé dans Git au plus tard le 16 Octobre 2026)

### Exigences de l'équipe
| Id | Exigence (reformulée par l'équipe) | Critère d'acceptation | Hypothèses |
|---|---|---|---|
| E1 | Initialisation | Après NRST ou mise sous tension : état ARRÊT en moins de 100 ms (DEL rouge seule, CCR1 = 0, AI1 = AI2 = 0, STB = 0, buzzer silencieux), moteur immobile, TIM1 à 20 kHz et SysTick à 1 ms actifs. | Horloge MSI de 4 MHz par défaut (SystemInit vide), pas de HAL; `motor_init` laisse STB = 0 et CCR1 = 0 avant que TIM1 compte.|
| E2 | PWM moteur | TIM1_CH1 (PE9) en PWM 1 avec prérechargement et MOE; fréquence 20 kHz ± 5 %; rapport cyclique de 0 à 100 % (au moins 100 pas) mesuré à ± 2 points (T3). | HCLK = PCLK2 = 4 MHz, donc PSC = 0 et ARR = 199; CCR1 = pct × 200 / 100 (résolution de 0,5 %).|
| E3 | Sens de marche | AVANT : AI1 = 1, AI2 = 0, STB = 1. ARRIÈRE : AI1 = 0, AI2 = 1, STB = 1. ARRÊT et URGENCE : CCR1 = 0, AI1 = AI2 = 0, STB = 0. Changement de sens seulement par ARRÊT, jamais avec CCR1 ≠ 0; AI1 = AI2 = 1 jamais produit. | MARCHE_AVANT = AI1 = 1, AI2 = 0 (sens réel de rotation relevé à la séance 1); alternance par `prochain_sens`; règle appliquée dans `motor_set_dir` (refus si CCR1 ≠ 0). |
| E4 | Consigne et rampe | Consigne de 25, 50, 75 ou 100 % (50 % au départ); chaque appui validé sur Touch En passe à la suivante, avec un bip court. Rampe de 1 point par 10 ms (0 → 100 % en 1 s ± 10 %) à la montée et à la descente; User vers ARRÊT coupe la PWM sans rampe. | Rampe cadencée par `RAMPE_PAS_MS` = 10 dans `fsm_maj_del`; `CONSIGNE_PAS` = 25; `pwm_pct` remis à 0 à chaque changement d'état. |
| E5 | Encodeur par interruption | EXTI0 sur les deux fronts de E_A, de priorité entre E-Stop et SysTick; sens déduit de A et B, `enc_count` signé de 32 bits; aucun front perdu (environ 4 400 par seconde); ISR de moins de 15 µs. | `ENC_PUPDR` = 0 (sorties actives de l'encodeur); `ENC_SENS_INVERSE` = 0 à confirmer à T5; durée de l'ISR estimée à environ 50 cycles (≈ 12 µs à 4 MHz), à mesurer. |
| E6 | Mesure de vitesse et de sens | Toutes les 500 ms : `vitesse_fronts` (valeur absolue) et `sens_mesure` (0 immobile, 1 avant, 2 arrière) mis à jour; tr/min = fronts × 120 / 748; sens mesuré comparé au sens commandé. | `encoder_read` lit `enc_count` en une seule instruction (pas de masquage d'interruption); fenêtre comptée par `fenetre_compteur`; référence : 348 tr/min mesurés à vide à 100 % (arbre de sortie). |
| E7 | E-Stop coupe la propulsion dans l’interruption | EXTI2 (priorité 0) met CCR1 = 0, STB = 0, AI1 = AI2 = 0, DEL rouge seule, buzzer à 1 et `estop_flag` = 1; la PWM disparaît moins de 1 ms après le front sur PB2, même pendant une rampe. | L'ISR sauvegarde LR (`push {r4, lr}`) et appelle `motor_stop_now` en premier; si la rampe réécrit CCR1 juste après, STB = 0 garde le pilote en veille (état sûr).. |
| E8 | Signalisation d’urgence et acquittement | Rouge à 2 Hz ± 10 %, buzzer continu, STB = 0, User et Touch En (consigne) ignorés. Touch En validé avec E-Stop relâché ramène en ARRÊT (buzzer coupé, consigne conservée); refusé si E-Stop maintenu; aucune reprise automatique. | Acquittement = `button_pressed(BTN_TOUCH)` et `button_raw(BTN_ESTOP)` = 0; `buzzer_continu` mis à 1 à l'urgence, remis à 0 à l'acquittement; demi-période de 250 ms (`CLIGNOTEMENT_DEMI_MS`). |
| E9 | Buzzer | Bip de 100 ms ± 10 % à chaque changement d'état et de consigne; double bip en MARCHE_ARRIÈRE; son continu en urgence; sans blocage, à partir de la base de temps SysTick. | `buzzer_bip` prépare la séquence et `buzzer_step` l'avance chaque ms (`BIP_MS` = `BIP_SILENCE_MS` = 100); commande GPIO sur PA0 (pas de variante TIM2); durée réelle d'environ 99 ms. |
|E10|Base de temps SysTick| LOAD = 3999 pour une période de 1 ms ± 1 %; `SysTick_Handler` incrémente seulement `ticks_ms` et positionne `tick_1ms`; `fsm_step` exécutée une fois par tick; `delay_ms` réécrite sur `ticks_ms`; aucune attente calibrée ne subsiste. | `HORLOGE_HZ` = 4 000 000 donc LOAD = 4 × 1000 − 1; `main` attend `tick_1ms` puis appelle `fsm_step`; `delay_ms` absente de la boucle principale; durée de `fsm_step` inférieure à 1 ms, à vérifier au débogueur. |
|E11|Mesures aux points de test| Mesures à l'oscilloscope : Fréquence et cycle de travail du PWM pour 25%,50%,75% et 100% de cycle de travail ;Niveaux AI1/AI2/STB en fonction de l'état ;E_A et E_B dans les deux sens ;Délai d'arrêt E-Stop → PWM égal à zéro ;Temps de génération du bip. Chaque résultat est mesuré par rapport à la spécification. |Mesures à vide, sonde ×10, masse sur GND du connecteur de points de test (STE 2061); incertitude de lecture de l'oscilloscope consignée.|
|E12|Robustesse| E-Stop, Touch En et User pendant une rampe, appuis simultanés, E-Stop pendant l'ISR de l'encodeur : jamais deux DEL allumées, jamais AI1 = AI2 = 1, jamais de changement de sens avec CCR1 ≠ 0, jamais de sortie d'urgence sans acquittement, `enc_count` non corrompu | Protections logicielles : refus dans `motor_set_dir`, STB = 0 en urgence, variables partagées de 32 bits lues et écrites en une instruction; essais moteur alimenté, plateforme à plat. |

### Rôles et rotation
| Séance | Réalise | Valide (essais, mesures, relecture) |
|---|---|---|
| Séance 0 | Rawda Massaoudou Tahirou | Alice Mpabuye |
| Séance 1 & Démonstration (13 sept.) | Alice Mpabuye | Rawda Massaoudou Tahirou |

### Échéancier des laboratoires 1 à 5
| Laboratoire | Séances | Démonstration | Remise | Responsable du suivi |
|---|---|---|---|---|
| 1 | 2 seances | 22 septembre 2026 | 9 octobre 2026 | Rawda & Alice |
| 2 | Octobre 2026 | Octobre 2026 | 2 Novembre 2026 | Rawda & Alice |
| 3 | Novembre 2026 | Novembre 2026 | Novembre 2026 | Rawda & Alice |
| 4 | Novembre 2026 | Décembre 2026 | Décembre 2026 | Rawda & Alice |
| 5 | Décembre 2026 | Décembre 2026 | Décembre 2026 | Rawda & Alice |

## Journal des séances

### Séance 1 — 13 Octobre 2026 — réalise : Rawda Massaoudou Tahirou / valide : Alice Mpabuye
- **Objectifs** : 
- **Fait** : 
- **Décisions** : 
- **Difficultés et solutions** : 
- **Essais et mesures** : 
- **Validations Git** : 

### Séance 2 — 22 Octobre 2026 — réalise : Alice Mpabuye / valide : Rawda Massaoudou Tahirou
- **Objectifs** :
- **Fait** :
- **Décisions** :
- **Difficultés et solutions** :
- **Essais et mesures** :
- **Validations Git** :

## Tableau des essais (T1 à T10)
| Essai | Date | Résultat observé | Verdict | Preuve (fichier) |
|---|---|---|---|---|
| T1 Réinitialisation |  |  |  |
| T2 Cycle User et sens |  |  | |
| T3 PWM |  |  |  |
| T4 Rampe |  |  |  |
| T5 Encodeur |  | |  |
| T6 Vitesse |  |  |  |
| T7 E-Stop |  |  |  |  |
| T8 Urgence et acquittement |  |  |  |  |
| T9 Buzzer |  |  |  |  |
| T10 SysTick et robustesse |  |  |  |  |

## Routine conservée pour L3-A
- **Routine** : `button_pressed` (implémentée dans `buttons.s`)
- **Interface** : Reçoit l'identifiant du bouton dans `r0` (`0` pour User, `1` pour E-Stop, `2` pour Touch En) et retourne `1` si un front d'appui valide est détecté, `0` sinon[cite: 5, 8]. Respecte entièrement les conventions AAPCS avec sauvegarde de `r4-r5-r6-lr`[cite: 8].
- **Cas d'essai** : Soumission d'une suite d'impulsions bruitées (rebonds simulés de < 30 ms) pour vérifier que le filtre ignore les instabilités et ne déclenche qu'un seul front validé après le seuil temporel. Cette routine sera réécrite en langage C au Laboratoire 3 pour comparaison de performance[cite: 3, 6].

## Déclaration des sources et de l'usage d'outils d'IA générative
- **Sources** : Manuel de référence STM32L5 (RM0438), Guide de la plateforme Leafy, Notes de cours
- **Outils d'IA** : Aucun usage d'outil d'IA générative