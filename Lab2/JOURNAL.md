# JOURNAL.md — Journal d'équipe, CEG 3536, laboratoire 2 (automne 2026)

Équipe 2 : Rawda Massaoudou Tahirou et Alice Mpabuye — Section : A02 (Groupe 2) — Dépôt Git : https://github.com/ShotReady/CEG3536_equipeRawda-Alice

## Suivi du jalon J1

| Laboratoire | Échéancier prévu (J1) | Déroulement réel | Écart et cause |
|---|---|---|---|
| 1 | Séances en septembre ; démonstration le 22 sept. 2026 ; remise le 9 oct. 2026 | Démonstration validée le 22 sept. 2026 (T1 à T10 conformes) ; remise sur Brightspace le 9 oct. 2026 | Aucun écart constaté |
| 2 | Séances en octobre ; démonstration à la séance 2 (20 oct.) ; remise le 2 nov. 2026 | Contenu des séances 1 et 2 réalisé le 13 oct. 2026 ; démonstration faite devant l'assistant le 13 oct. 2026 (T1 à T10 conformes) ; rapport à remettre le 2 nov. 2026 | Démonstration faite une semaine avant la date prévue, car le travail a été terminé à la première séance |

## Rôles et rotation

| Séance | Réalise | Valide (essais, mesures, relecture) | Périphérique configuré ou mesuré par chacun |
|---|---|---|---|
| Préparation (12 oct.) | Rawda : systick.s, motor.s, buttons.s, main.s, fsm.s. Alice : encoder.s, buzzer.s, estop.s | Compilation du projet complet par Rawda (0 erreur, 0 avertissement) | Rawda : TIM1 (PWM) et SysTick. Alice : EXTI0 (encodeur) |
| Séance 1 (13 oct.) | Rawda et Alice ont réalisé le laboratoire ensemble | Rawda et Alice ensemble | Les deux membres ont travaillé sur les trois périphériques (TIM1, SysTick, EXTI0) |
| Séance 2 (20 oct.) | Aucune activité : le contenu a été terminé le 13 oct. | — | — |

## Calculs préalables (avant la séance 1)

Hypothèse commune : SystemInit vide, oscillateur MSI à 4 MHz après réinitialisation (HCLK = PCLK1 = PCLK2 = 4 MHz).

| Grandeur | Formule | Valeur | Vérifiée par (essai) |
|---|---|---|---|
| `PWM_PSC`, `PWM_ARR` | f = 4 MHz / ((PSC + 1)(ARR + 1)) = 20 kHz → (ARR + 1) = 4 000 000 / (1 × 20 000) = 200 | PSC = 0, ARR = 199 (0xC7) ; période 50 µs | T3 |
| Résolution du rapport cyclique | 1 / (ARR + 1) | 0,5 % par pas (200 pas, plus de 100 pas exigés) | T3 |
| `CCR1` pour 25 / 50 / 75 / 100 % | pct × (ARR + 1) / 100 | 50 / 100 / 150 / 200 | T3 |
| `SYSTICK_LOAD_1MS` | 4 MHz × 1 ms − 1 | 3999 (0xF9F) | T10 |
| Priorités (3 bits utiles, valeur << 5) | E-Stop 0, encodeur 1, SysTick 2 | octets 0x00 (IPR 13), 0x20 (IPR 11), 0x40 (SHPR3, octet 0xE000ED23) | Débogueur |
| Fronts par tour (arbre de sortie) | 2 fronts × 11 impulsions × 34 (réducteur) | 748 | T5 |
| tr/min à partir de `vitesse_fronts` | (fronts / 748) × (60 / 0,5 s) = fronts × 120 / 748 | 1 front ≈ 0,16 tr/min | T6 |
| Fronts attendus par fenêtre de 500 ms à 100 % | 350 tr/min × 748 / 120 | ≈ 2 182 (l'énoncé donne ≈ 2 200 ; 2 167 à 348 tr/min selon l'énoncé) | T6 |
| Fréquence des interruptions EXTI0 à 100 % | (350 / 60) tr/s × 748 fronts | ≈ 4 363 par seconde (≈ 4 400) | T5 |
| Rampe 0 → 100 % | 100 pas × RAMPE_PAS_MS (10 ms) | 1,00 s ± 10 % | T4 |
| Rampe 100 → 25 % | 75 pas × 10 ms | 750 ms ± 10 % | T4 |
| Clignotement d'urgence | demi-période 250 ms | 2 Hz (période 500 ms) ± 10 % | T8 |
| Bip | BIP_MS = 100 ms ; silence entre bips 100 ms | 100 ms ± 10 % (le compteur donne 99 ms, dans la tolérance) | T9 |
| Délai E-Stop → CCR1 = 0 (estimation) | latence d'interruption (≈ 12 cycles) + `push {r4, lr}`, `bl motor_stop_now`, chargement de l'adresse de TIM1 et écriture de CCR1 (≈ 12 cycles) ≈ 24 cycles ; puis au plus 50 µs (une période PWM) pour que le prérechargement soit appliqué | ≈ 6 µs à 4 MHz jusqu'à l'écriture de CCR1, au plus ≈ 56 µs jusqu'à la disparition de la PWM ; exigence < 1 ms | T7 |

### Valeurs attendues au débogueur (fenêtre SFRs)

| Registre | Valeur attendue |
|---|---|
| TIM1_CR1 | 0x81 (ARPE, CEN) |
| TIM1_CCMR1 | 0x68 (OC1M = PWM 1, OC1PE) |
| TIM1_CCER | 0x1 (CC1E) |
| TIM1_ARR | 0xC7 (199) |
| TIM1_BDTR | 0x8000 (MOE) |
| SysTick LOAD / CTRL | 0xF9F / 0x7 |
| SCB_SHPR3 | 0x40000000 |
| NVIC_ISER0 | bits 11 (EXTI0) et 13 (EXTI2) |
| NVIC_IPR octets 11 et 13 | 0x20 et 0x00 |
| EXTI_EXTICR1 | 0x00010001 (lignes 0 et 2 sur le port B) |
| EXTI_RTSR1 / FTSR1 / IMR1 | RTSR1 bits 0 et 2 ; FTSR1 bit 0 ; IMR1 bits 0 et 2 |

### Niveaux AI1 / AI2 / STB par état (ODR de GPIOB bits 10 et 11, ODR de GPIOE bit 15)

Les niveaux de MARCHE_AVANT et de MARCHE_ARRIÈRE sont ceux appliqués par `motor_set_dir` (voir la séance 1, Décisions).

| État | AI1 (PB10) | AI2 (PB11) | STB (PE15) | CCR1 | Résultat |
|---|---|---|---|---|---|
| ARRÊT | 0 | 0 | 0 | 0 | Conforme (T2) |
| MARCHE_AVANT | 0 | 1 | 1 | rampe vers la consigne | Conforme (T2) |
| MARCHE_ARRIÈRE | 1 | 0 | 1 | rampe vers la consigne | Conforme (T2) |
| ARRÊT_URGENCE | 0 | 0 | 0 | 0 | Conforme (T7, T8) |

### Table de fonctionnement du pilote de moteur (guide, table 12)

| AI1 | AI2 | STB | Mode | Utilisé dans le projet |
|---|---|---|---|---|
| 1 | 0 | 1 | Rotation dans un sens (avec PWM) | MARCHE_ARRIÈRE |
| 0 | 1 | 1 | Rotation dans l'autre sens (avec PWM) | MARCHE_AVANT |
| 0 | 0 | 1 | Arrêt, sorties en haute impédance | Non utilisé (STB est mis à 0) |
| 1 | 1 | 1 | Freinage court | Jamais produit |
| x | x | 0 | Veille | ARRÊT et ARRÊT_URGENCE |

## Journal des séances

### Préparation — 12 octobre 2026
- Objectifs : projet du laboratoire 2 compilé avant la séance 1, calculs préalables faits.
- Fait :
  - Copie du projet du laboratoire 1 en `CEG3536_Lab2`, ajout de `motor.s`, `encoder.s`, `systick.s`, `buzzer.s`, `registres_lab2.inc` et `lab2.h`.
  - `delay_ms` retirée de `buttons.s` (elle est maintenant dans `systick.s`).
  - Première moitié (Rawda) : `systick.s`, `motor.s`, `buttons.s`, `main.s`, `fsm.s`. Seconde moitié (Alice) : `encoder.s`, `buzzer.s`, `estop.s`.
  - Compilation du projet complet : 0 erreur, 0 avertissement. Aucun essai sur le matériel à cette date.
- Décisions :
  - Répartition des fichiers entre les deux membres pour éviter les conflits dans Git (voir tableau des rôles).
  - Correction d'un défaut du laboratoire 1 dans `estop_init` : les deux branches écrivaient `EXTI_FPR1` (registre de requête) au lieu de `EXTI_FTSR1` (front descendant). Sans effet sur notre carte (E-Stop actif haut), corrigé par sécurité.
  - `motor_set_dir` refuse aussi un passage direct avant ↔ arrière (en plus du refus quand CCR1 ≠ 0).
  - `fsm_init` est appelée avant `systick_init` dans `main` : l'état sûr est établi avant le premier tick.
  - Sens de comptage de l'encodeur : A = B compte +1 (`ENC_SENS_INVERSE = 0`).
- Difficultés et solutions : faute de frappe sur la constante `CONSIGNE_INITIALE` dans `fsm.s` (erreur « undefined symbol »), corrigée.

### Séance 1 — 13 octobre 2026 — Rawda et Alice ensemble
- Objectifs : tout le contenu prévu aux séances 1 et 2 : sécurité du banc, TIM1 et PWM (T3), sens de marche et sens réel de rotation, SysTick (T10), bips (T9), propulsion raccordée à la machine à états (E1 à E4, E9, E10), encodeur et vitesse (T5, T6), coupure E-Stop → PWM (T7), urgence et acquittement (T8), robustesse (T10), démonstration.
- Fait : tous les objectifs ci-dessus ont été réalisés le jour même, essais T1 à T10 conformes. À la démonstration, l'assistant a vérifié la marche avant, la marche arrière, l'accélération (rampe), l'arrêt d'urgence et l'arrêt. Les deux membres étaient présents pendant toute la démonstration.
- Décisions : sur indication de l'assistant, les niveaux de AI1 et AI2 appliqués par `motor_set_dir` ont été échangés pour que la marche avant fasse avancer le véhicule vers l'avant et la marche arrière vers l'arrière. MARCHE_AVANT applique AI1 = 0 et AI2 = 1, MARCHE_ARRIÈRE applique AI1 = 1 et AI2 = 0, et le test de refus d'un passage direct avant ↔ arrière a été échangé de la même façon. Les broches (PB10, PB11) sont inchangées. `ENC_SENS_INVERSE` est resté à 0.
- Difficultés et solutions : le sens de marche obtenu ne correspondait pas au sens attendu du véhicule ; corrigé par la modification de `motor_set_dir` décrite ci-dessus. C'est la seule modification du code faite après la préparation du 12 octobre.
- Mesures au débogueur : les valeurs observées sont celles prévues par les constantes du code (PSC, ARR, CCR1, LOAD, priorités) ; aucune valeur n'a été consignée séparément (voir le tableau des mesures).
- Validations Git : Rawda a validé le code complet du laboratoire 2 ; Alice a validé sa partie du code.

### Séance 2 — 20 octobre 2026
- Séance non tenue : le contenu prévu (encodeur, vitesse, E-Stop, urgence, robustesse, démonstration) a été réalisé le 13 octobre (voir ci-dessus).

## Tableau des mesures (composante L2-A)

Les valeurs ci-dessous sont calculées d'après les constantes du code (PSC, ARR, CCR1, LOAD, RAMPE_PAS_MS, BIP_MS) ou constatées conformes à la démonstration. Aucune capture du débogueur n'a été prise (l'assistant n'en exigeait pas) : la mention « non relevée » indique une valeur qui n'a pas été notée.

| Mesure | Exigence | Valeur | Incertitude / méthode | Écart | Cause | Preuve |
|---|---|---|---|---|---|---|
| Fréquence PWM | 20 kHz ± 5 % | 20 kHz (PSC = 0, ARR = 199) | Calculée d'après PSC et ARR (vue SFR de TIM1) | 0 % (théorique) | Précision du MSI non vérifiée | Calcul d'après le code |
| Rapport cyclique à 25 / 50 / 75 / 100 % | ± 2 points | 25 / 50 / 75 / 100 % (CCR1 = 50 / 100 / 150 / 200) | Calculé : CCR1 / (ARR + 1) | 0 point | Résolution de 0,5 % ; conversion exacte (entiers) | Calcul d'après le code |
| Rampe 0 → 100 % | 1 s ± 10 % | 1,00 s (100 pas × 10 ms) | Calculée d'après RAMPE_PAS_MS ; conforme à la démonstration ; `ticks_ms` non relevé | 0 (théorique) | Rampe cadencée par SysTick (1 ms) | Démonstration du 13 oct. |
| Rampe 100 → 25 % | 750 ms ± 10 % | 750 ms (75 pas × 10 ms) | Idem | 0 (théorique) | Idem | Démonstration du 13 oct. |
| Période SysTick | 1 ms ± 1 % | 1 ms (LOAD = 3999 à 4 MHz) | Calculée d'après LOAD ; `ticks_ms` sur 10 s non relevé | 0 % (théorique) | Précision du MSI non vérifiée | Calcul d'après le code |
| Vitesse à 25 / 50 / 75 / 100 % (avant) | croissante, stable ± 10 % | Non relevée ; conforme à la démonstration | `vitesse_fronts` en Live Expressions | — | — | Démonstration du 13 oct. |
| Vitesse à 25 / 50 / 75 / 100 % (arrière) | idem | Non relevée ; conforme à la démonstration | idem | — | — | Démonstration du 13 oct. |
| Évolution de `enc_count` (avant / arrière) | croît dans un sens, décroît dans l'autre | Conforme à la démonstration ; valeurs non relevées | Live Expressions, moteur à 100 % | — | — | Démonstration du 13 oct. |
| Fronts comptés sur 1 s à 100 % | ≈ 4 400 | Non relevé | Différence de `enc_count` sur 1 s | — | — | — |
| Délai E-Stop → PWM nulle | < 1 ms | ≈ 6 µs jusqu'à l'écriture de CCR1, au plus ≈ 56 µs jusqu'à la disparition de la PWM (estimation) | Estimation d'après la latence d'interruption (≈ 12 cycles) et les instructions avant l'écriture de CCR1 (≈ 24 cycles au total à 4 MHz) ; coupure constatée conforme à la démonstration | Très inférieur à 1 ms | Latence d'interruption et prérechargement de CCR1 | Démonstration du 13 oct. |
| Durée d'un bip | 100 ms ± 10 % | 99 ms (compteur `buzzer_restant` de 100 ms décrémenté chaque ms) | Calcul d'après le code ; `ticks_ms` non relevé | −1 ms | Le premier décompte a lieu au pas suivant | Calcul d'après le code |
| Période de clignotement d'urgence | 500 ms ± 10 % | 500 ms (2 × 250 ms) | Calculée d'après CLIGNOTEMENT_DEMI_MS ; conforme à la démonstration | 0 (théorique) | Cadencée par SysTick (1 ms) | Démonstration du 13 oct. |

### Vitesse en fonction de la consigne

Les valeurs de `vitesse_fronts` n'ont pas été relevées pendant la séance. Valeur de référence à 100 % : ≈ 2 182 fronts par fenêtre de 500 ms (≈ 350 tr/min). Le comportement observé à la démonstration, dans les deux sens, est conforme : valeurs stables et croissantes avec la consigne, sens mesuré conforme au sens commandé.

## Tableau des essais (T1 à T10)

| Essai | Date | Résultat observé | Verdict | Preuve |
|---|---|---|---|---|
| T1 Réinitialisation | 13 oct. 2026 | ARRÊT : rouge seule, CCR1 = 0, STB = 0, moteur immobile, buzzer silencieux | Conforme | Démonstration devant l'assistant |
| T2 Cycle User et sens | 13 oct. 2026 | Rouge → verte → rouge → bleue → rouge ; AI1 / AI2 / STB conformes (voir Décisions) ; sens du moteur conforme | Conforme | Démonstration devant l'assistant |
| T3 PWM | 13 oct. 2026 | 20 kHz ; rapport cyclique conforme à 25, 50, 75 et 100 % | Conforme | Démonstration devant l'assistant |
| T4 Rampe | 13 oct. 2026 | Rampe de 1 s à la montée et de 750 ms à la descente | Conforme | Démonstration devant l'assistant |
| T5 Encodeur | 13 oct. 2026 | `enc_count` croît dans un sens et décroît dans l'autre ; sens mesuré conforme au sens commandé | Conforme | Démonstration devant l'assistant |
| T6 Vitesse | 13 oct. 2026 | Valeurs stables et croissantes avec la consigne, dans les deux sens | Conforme | Démonstration devant l'assistant |
| T7 E-Stop | 13 oct. 2026 | PWM nulle et STB = 0 immédiatement, buzzer continu, `estop_flag` = 1 | Conforme | Démonstration devant l'assistant |
| T8 Urgence et acquittement | 13 oct. 2026 | User ignoré ; acquittement refusé avec E-Stop maintenu puis accepté ; rouge fixe, buzzer coupé | Conforme | Démonstration devant l'assistant |
| T9 Buzzer | 13 oct. 2026 | Bip court, double bip en marche arrière, bip court au changement de consigne | Conforme | Démonstration devant l'assistant |
| T10 SysTick et robustesse | 13 oct. 2026 | Période de 1 ms ; système cohérent dans tous les cas de robustesse | Conforme | Démonstration devant l'assistant |

## Routine conservée pour L3-A
- Routine : `motor_set_pwm` (motor.s). Choix : fonction feuille courte, déterministe et sans dépendance externe, donc facile à tester par valeurs d'entrée et à comparer à une version C (taille du code, nombre de cycles).
- Interface : R0 = rapport cyclique demandé, 0 à 100 (%) ; valeurs au-dessus de 100 saturées à 100. Retour R0 = valeur écrite dans TIM1_CCR1 (CCR1 = pct × 200 / 100). Utilise R0 à R2 seulement, aucun registre R4 à R11 modifié, pas de pile.
- Cas d'essai (valeurs attendues) : 0 → 0 ; 25 → 50 ; 50 → 100 ; 75 → 150 ; 100 → 200 ; 101 → 200 (saturation) ; 255 → 200 (saturation).
- Résultats obtenus : les valeurs attendues pour 25, 50, 75 et 100 % sont conformes (T3).

## Déclaration des sources et de l'usage d'outils d'IA générative
- Sources : projet du laboratoire 1 de l'équipe ; complément de démarrage CEG3536_Lab2_Complement.zip (Brightspace) ; manuel de référence RM0438 ; PM0264 ; guide de la plateforme Leafy ; manuel Zhu, 4e éd. ; fichiers de démarrage
- Outils d'IA : aucun outil utilise pour la realisation de ce laboratoire 
