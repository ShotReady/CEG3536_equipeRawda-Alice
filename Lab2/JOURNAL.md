# JOURNAL.md — Journal d'équipe, CEG 3536, laboratoire 2 (automne 2026)

Équipe 2 : Rawda Massaoudou Tahirou et Alice Mpabuye — Section : A02 (Groupe 2) — Dépôt Git : https://github.com/ShotReady/CEG3536_equipeRawda-Alice

Projet : `CEG3536_Lab2_<équipe>` (copie du projet du laboratoire 1, complément de démarrage importé). Branche ou dossier Git : `lab2`.

## Suivi du jalon J1

| Laboratoire | Échéancier prévu (J1) | Déroulement réel | Écart et cause | Ajustement pour les laboratoires 3 à 5 |
|---|---|---|---|---|
| 1 | Séances en septembre ; démonstration le 22 sept. 2026 ; remise le 9 oct. 2026 | Démonstration validée le 22 sept. 2026 (T1 à T10 conformes) ; remise sur Brightspace le 9 oct. 2026 | Aucun écart constaté | `<à remplir>` |
| 2 | Séances en octobre ; démonstration à la séance 2 (20 oct.) ; remise le 2 nov. 2026 | `<à remplir après la démonstration>` | `<à remplir>` | `<à remplir>` |

## Rôles et rotation

| Séance | Réalise | Valide (essais, mesures, relecture) | Périphérique configuré ou mesuré par chacun |
|---|---|---|---|
| Préparation (12 oct.) | Rawda : systick.s, motor.s, buttons.s, main.s, fsm.s. Alice : encoder.s, buzzer.s, estop.s | Relecture croisée des fichiers de l'autre membre | Rawda : TIM1 (PWM) et SysTick. Alice : EXTI0 (encodeur) |
| Séance 1 (13 oct.) | Rawda et Alice, en alternance (pas de rôle fixe) | Rawda et Alice, en alternance | `<à remplir : qui a configuré ou mesuré quoi>` |
| Séance 2 (20 oct.) | Rawda et Alice, en alternance (pas de rôle fixe) | Rawda et Alice, en alternance | `<à remplir : qui a configuré ou mesuré quoi>` |

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
| Fronts attendus par fenêtre de 500 ms à 100 % | 350 tr/min × 748 / 120 | ≈ 2 182 (le guide donne ≈ 2 200 ; 2 167 mesurés à 348 tr/min) | T6 |
| Fréquence des interruptions EXTI0 à 100 % | (350 / 60) tr/s × 748 fronts | ≈ 4 363 par seconde (≈ 4 400) | T5 |
| Rampe 0 → 100 % | 100 pas × RAMPE_PAS_MS (10 ms) | 1,00 s ± 10 % | T4 |
| Rampe 100 → 25 % | 75 pas × 10 ms | 750 ms ± 10 % | T4 |
| Clignotement d'urgence | demi-période 250 ms | 2 Hz (période 500 ms) ± 10 % | T8 |
| Bip | BIP_MS = 100 ms ; silence entre bips 100 ms | 100 ms ± 10 % (le compteur donne 99 ms, dans la tolérance) | T9 |

### Valeurs attendues au débogueur (fenêtre SFRs)

| Registre | Valeur attendue | Observé |
|---|---|---|
| TIM1_CR1 | 0x81 (ARPE, CEN) | |
| TIM1_CCMR1 | 0x68 (OC1M = PWM 1, OC1PE) | |
| TIM1_CCER | 0x1 (CC1E) | |
| TIM1_ARR | 0xC7 (199) | |
| TIM1_BDTR | 0x8000 (MOE) | |
| SysTick LOAD / CTRL | 0xF9F / 0x7 | |
| SCB_SHPR3 | 0x40000000 | |
| NVIC_ISER0 | bits 11 (EXTI0) et 13 (EXTI2) | |
| NVIC_IPR octets 11 et 13 | 0x20 et 0x00 | |
| EXTI_EXTICR1 | 0x00010001 (lignes 0 et 2 sur le port B) | |
| EXTI_RTSR1 / FTSR1 / IMR1 | RTSR1 bits 0 et 2 ; FTSR1 bit 0 ; IMR1 bits 0 et 2 | |

### Table de fonctionnement du pilote de moteur (guide, table 12) — à relever à la séance 1

| AI1 | AI2 | STB | Mode | Utilisé dans le projet |
|---|---|---|---|---|
| 1 | 0 | 1 | Rotation dans un sens (avec PWM) | MARCHE_AVANT |
| 0 | 1 | 1 | Rotation dans l'autre sens (avec PWM) | MARCHE_ARRIÈRE |
| 0 | 0 | 1 | Arrêt, sorties en haute impédance | Non utilisé (STB est mis à 0) |
| 1 | 1 | 1 | Freinage court | Jamais produit |
| x | x | 0 | Veille | ARRÊT et ARRÊT_URGENCE |

## Journal des séances

### Préparation — 12 octobre 2026
- Objectifs : projet du laboratoire 2 compilé avant la séance 1, calculs préalables faits.
- Fait :
  - Copie du projet du laboratoire 1 en `CEG3536_Lab2_<équipe>`, ajout de `motor.s`, `encoder.s`, `systick.s`, `buzzer.s`, `registres_lab2.inc` et `lab2.h`.
  - `delay_ms` retirée de `buttons.s` (elle est maintenant dans `systick.s`).
  - Première moitié (Rawda) : `systick.s`, `motor.s`, `buttons.s`, `main.s`, `fsm.s`. Seconde moitié (Alice) : `encoder.s`, `buzzer.s`, `estop.s`.
  - Compilation du projet complet : 0 erreur, 0 avertissement. Aucun essai sur le matériel à ce jour.
- Décisions :
  - Répartition des fichiers entre les deux membres pour éviter les conflits dans Git (voir tableau des rôles).
  - Correction d'un défaut du laboratoire 1 dans `estop_init` : les deux branches écrivaient `EXTI_FPR1` (registre de requête) au lieu de `EXTI_FTSR1` (front descendant). Sans effet sur notre carte (E-Stop actif haut), corrigé par sécurité.
  - `motor_set_dir` refuse aussi un passage direct avant ↔ arrière (en plus du refus quand CCR1 ≠ 0).
  - `fsm_init` est appelée avant `systick_init` dans `main` : l'état sûr est établi avant le premier tick.
  - Sens de comptage de l'encodeur : A = B compte +1 (`ENC_SENS_INVERSE = 0`). À confirmer à l'essai T5.
- Difficultés et solutions : faute de frappe sur la constante `CONSIGNE_INITIALE` dans `fsm.s` (erreur « undefined symbol »), corrigée.
- Validations Git (auteur, message) : `<à remplir avec git log : auteur et message de chaque validation>`

### Séance 1 — 13 octobre 2026 — réalisation et validation partagées entre Rawda et Alice
- Objectifs : vérifier le banc (sécurité), TIM1 et PWM (T3), sens de marche et sens de rotation réel, SysTick (T10), bips (T9), propulsion raccordée à la machine à états (E1 à E4, E9, E10).
- Fait :
- Décisions (sens réel de rotation pour AI1 = 1, AI2 = 0 ; `ENC_SENS_INVERSE` ; variante buzzer) :
- Difficultés et solutions :
- Mesures (PWM, SysTick, bips) :
- Validations Git (auteur, message) :

### Séance 2 — 20 octobre 2026 — réalisation et validation partagées entre Rawda et Alice
- Objectifs : encodeur et vitesse (T5, T6), délai E-Stop → PWM (T7), urgence et acquittement (T8), robustesse (T10), démonstration.
- Fait :
- Décisions :
- Difficultés et solutions :
- Mesures (encodeurs, vitesse, délai E-Stop) :
- Validations Git :

## Tableau des mesures (composante L2-A)

| Mesure | Exigence | Valeur mesurée | Incertitude / méthode | Écart | Cause | Preuve (fichier) |
|---|---|---|---|---|---|---|
| Fréquence PWM | 20 kHz ± 5 % | | | | | |
| Rapport cyclique à 25 / 50 / 75 / 100 % | ± 2 points | | | | | |
| Rampe 0 → 100 % | 1 s ± 10 % | | | | | |
| Rampe 100 → 25 % | 750 ms ± 10 % | | | | | |
| Période SysTick | 1 ms ± 1 % | | | | | |
| Vitesse à 25 / 50 / 75 / 100 % (avant) | croissante, stable ± 10 % | | | | | |
| Vitesse à 25 / 50 / 75 / 100 % (arrière) | idem | | | | | |
| Déphasage E_A / E_B (avant, arrière) | quadrature, inversé selon le sens | | | | | |
| Délai E-Stop → PWM nulle | < 1 ms | | | | | |
| Durée d'un bip | 100 ms ± 10 % | | | | | |
| Période de clignotement d'urgence | 500 ms ± 10 % | | | | | |

### Vitesse en fonction de la consigne

| Consigne | Sens | `vitesse_fronts` (fenêtre 1) | (fenêtre 2) | (fenêtre 3) | tr/min (fronts × 120 / 748) | `sens_mesure` |
|---|---|---|---|---|---|---|
| 25 % | avant | | | | | |
| 50 % | avant | | | | | |
| 75 % | avant | | | | | |
| 100 % | avant | | | | | |
| 25 % | arrière | | | | | |
| 50 % | arrière | | | | | |
| 75 % | arrière | | | | | |
| 100 % | arrière | | | | | |

## Tableau des essais (T1 à T10)

| Essai | Date | Résultat observé | Verdict | Preuve (fichier) |
|---|---|---|---|---|
| T1 Réinitialisation | | | | |
| T2 Cycle User et sens | | | | |
| T3 PWM | | | | |
| T4 Rampe | | | | |
| T5 Encodeur | | | | |
| T6 Vitesse | | | | |
| T7 E-Stop | | | | |
| T8 Urgence et acquittement | | | | |
| T9 Buzzer | | | | |
| T10 SysTick et robustesse | | | | |

## Routine conservée pour L3-A
- Routine : `motor_set_pwm` (motor.s). Choix : fonction feuille courte, déterministe et sans dépendance externe, donc facile à tester par valeurs d'entrée et à comparer à une version C (taille du code, nombre de cycles).
- Interface : R0 = rapport cyclique demandé, 0 à 100 (%) ; valeurs au-dessus de 100 saturées à 100. Retour R0 = valeur écrite dans TIM1_CCR1 (CCR1 = pct × 200 / 100). Utilise R0 à R2 seulement, aucun registre R4 à R11 modifié, pas de pile.
- Cas d'essai (valeurs attendues) : 0 → 0 ; 25 → 50 ; 50 → 100 ; 75 → 150 ; 100 → 200 ; 101 → 200 (saturation) ; 255 → 200 (saturation).
- Résultats obtenus : `<à remplir après les essais>`

## Déclaration des sources et de l'usage d'outils d'IA générative
- Sources :projet du laboratoire 1 de l'équipe ; complément de démarrage CEG3536_Lab2_Complement.zip (Brightspace) ; manuel de référence RM0438 ; PM0264 ; guide de la plateforme Leafy ; manuel Zhu, 4e éd. ; fichiers de démarrage
- Outils d'IA :