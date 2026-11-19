/* ---------------------------------------------------------------------------
 * fsm.c — machine à états du véhicule en C (CEG 3536, laboratoire 3)
 *         — CORRIGÉ (remplace fsm.s du laboratoire 2)
 *
 * États et transitions du laboratoire 2, conservés :
 *   EV_USER   : ARRÊT -> AVANT / ARRIÈRE (alternance), AVANT / ARRIÈRE -> ARRÊT
 *   EV_ESTOP  : tout état -> ARRÊT_URGENCE (propulsion déjà coupée par l'ISR)
 *   EV_TOUCH  : en urgence, acquittement si E-Stop relâché ; sinon bascule
 *               touch_enabled + bip court (laboratoire 1, pour le laboratoire 4)
 *   EV_CONSIGNE (valeur = %) : mémorisée ; appliquée seulement en marche (E5)
 *   EV_VITESSE  (valeur = fronts) : conversion par la routine assembleur (E3)
 * Sorties (fsm_sorties, un seul point) : rampe 1 point / 10 ms, DEL (rouge
 * clignotante en urgence), buzzer_step.
 * Les variables etat, touch_enabled, compteur_transitions, consigne_pct et
 * pwm_pct déclarées dans lab1.h / lab2.h sont définies ici (fsm.s retiré).
 * ------------------------------------------------------------------------- */
#include "fsm.h"
#include "lab4.h"
#include "stm32l552xx.h"

/* variables déclarées extern volatile dans lab1.h / lab2.h */
volatile uint32_t etat;
volatile uint32_t touch_enabled;
volatile uint32_t compteur_transitions;
volatile uint32_t consigne_pct;
volatile uint32_t pwm_pct;

static uint32_t consigne_pot;       /* dernière position du potentiomètre (%) */
static uint32_t prochain_sens;      /* 0 : prochain départ en avant           */
static uint32_t rampe_compteur, clignote_compteur, clignote_phase;
static etat_vehicule_t expose;

static const uint8_t etat_vers_del[5]  = { LED_ROUGE, LED_VERTE, LED_BLEUE, LED_ROUGE, LED_ROUGE };
static const uint8_t etat_vers_dir[5]  = { MOTOR_ARRET, MOTOR_AVANT, MOTOR_ARRIERE, MOTOR_ARRET, MOTOR_ARRET };
static const uint8_t etat_vers_bips[5] = { 1u, 1u, 2u, 0u, 0u };

#define PIR_BROCHE      9u

static uint32_t mode, cause, plafond_pct, drapeaux;
static uint32_t sw_feu, sw_obstacle, sw_limitation, sw_fin, sw_defaut;
static uint32_t hum_limite, defauts_i2c, defaut_tp, bip_compteur;

static uint32_t en_marche(void)
{
    return etat == ETAT_MARCHE_AVANT || etat == ETAT_MARCHE_ARRIERE;
}

static uint32_t pir_niveau(void)
{
    return (GPIOB->IDR >> PIR_BROCHE) & 1u;
}

static uint32_t bit_module(uint32_t adr)
{
    switch (adr) {
    case I2C_ADR_RVB:      return 1u << 0;
    case I2C_ADR_DEL:      return 1u << 1;
    case I2C_ADR_COMMUT:   return 1u << 2;
    case I2C_ADR_LCD:      return 1u << 3;
    case I2C_ADR_TOUCHPAD: return 1u << 4;
    default:               return 0u;
    }
}

static uint32_t defaut_actif(void)
{
    return (mode == MODE_AUTONOME && sw_defaut) || defauts_i2c != 0u;
}

static uint32_t obstacle_actif(void)
{
    return pir_niveau() || (mode == MODE_AUTONOME && sw_obstacle);
}

static uint32_t feu_actif(void)
{
    return mode == MODE_AUTONOME && sw_feu;
}

static uint32_t limitation_active(void)
{
    return hum_limite || (mode == MODE_AUTONOME && sw_limitation);
}

static uint32_t cause_active(void)
{
    if (defaut_actif()) {
        return CAUSE_DEFAUT;
    }
    if (obstacle_actif()) {
        return CAUSE_OBSTACLE;
    }
    if (feu_actif()) {
        return CAUSE_FEU;
    }
    return CAUSE_AUCUNE;
}

static void entree_etat(uint32_t nouvel_etat)
{
    etat = nouvel_etat;
    compteur_transitions++;
    pwm_pct = 0u;                               /* la rampe repart de 0        */
    rampe_compteur = 0u;
    (void)motor_set_dir(etat_vers_dir[nouvel_etat]);  /* accepté : CCR1 = 0   */
    if (nouvel_etat == ETAT_MARCHE_AVANT || nouvel_etat == ETAT_MARCHE_ARRIERE) {
        consigne_pct = consigne_pot;            /* prise en compte au départ   */
    } else {
        consigne_pct = 0u;
    }
    buzzer_bip(etat_vers_bips[nouvel_etat]);
}

static void arret_evenement(uint32_t c)
{
    motor_stop_now();
    cause = c;
    bip_compteur = 0u;
    entree_etat(ETAT_ARRET_EVENEMENT);
    if (c != CAUSE_FEU) {
        buzzer_bip(1u);
    }
}

static void acquitter_urgence(void)
{
    if (button_raw(BTN_ESTOP) == 0u) {
        buzzer_continu = 0u;
        cause = CAUSE_AUCUNE;
        entree_etat(ETAT_ARRET);
    }
}

static void acquitter_evenement(void)
{
    if (cause_active() == CAUSE_AUCUNE) {
        cause = CAUSE_AUCUNE;
        defaut_tp = 0u;
        entree_etat(ETAT_ARRET);
    }
}

static void changer_mode(void)
{
    mode ^= 1u;
    sw_feu = sw_obstacle = sw_limitation = sw_fin = sw_defaut = 0u;
}

static void traiter_touche(uint32_t touche)
{
    if (!touch_enabled) {
        return;
    }
    if (touche <= 9u) {
        plafond_pct = (touche == 0u) ? 100u : touche * 10u;
        buzzer_bip(1u);
    } else if (touche == (uint32_t)TOUCHE_FN) {
        if (etat == ETAT_ARRET_URGENCE) {
            acquitter_urgence();
        } else if (etat == ETAT_ARRET_EVENEMENT) {
            acquitter_evenement();
        }
    } else if (touche == (uint32_t)TOUCHE_NEXT) {
        if (etat == ETAT_ARRET) {
            changer_mode();
            buzzer_bip(1u);
        }
    }
}

static void traiter_commutateur(uint32_t sw)
{
    if (mode != MODE_AUTONOME) {
        return;
    }
    switch (sw) {
    case SW_FEU:        sw_feu ^= 1u;        break;
    case SW_OBSTACLE:   sw_obstacle ^= 1u;   break;
    case SW_LIMITATION: sw_limitation ^= 1u; break;
    case SW_FIN:        sw_fin ^= 1u;        break;
    case SW_DEFAUT:     sw_defaut ^= 1u;     break;
    default:            break;
    }
}

static void traiter_defaut(uint32_t valeur)
{
    uint32_t bit = bit_module(valeur & ~DEFAUT_I2C_RETABLI);

    if ((valeur & DEFAUT_I2C_RETABLI) != 0u) {
        defauts_i2c &= ~bit;
    } else {
        defauts_i2c |= bit;
        if (bit == bit_module(I2C_ADR_TOUCHPAD)) {
            defaut_tp = 1u;
        }
    }
}

static void surveiller(void)
{
    uint32_t c = cause_active();

    if (en_marche()) {
        if (c != CAUSE_AUCUNE) {
            arret_evenement(c);
        } else if (mode == MODE_AUTONOME && sw_fin) {
            cause = CAUSE_FIN;
            entree_etat(ETAT_ARRET);
        }
    } else if (etat == ETAT_ARRET) {
        if (c == CAUSE_DEFAUT) {
            arret_evenement(c);
        }
    } else if (etat == ETAT_ARRET_EVENEMENT) {
        if (c != CAUSE_AUCUNE) {
            cause = c;
        }
    }
}

static void recalculer_consigne(void)
{
    uint32_t c;

    if (!en_marche()) {
        consigne_pct = 0u;
        return;
    }
    c = (mode == MODE_AUTONOME) ? CONSIGNE_AUTO_PCT : consigne_pot;
    if (c > plafond_pct) {
        c = plafond_pct;
    }
    if (limitation_active() && c > LIMITATION_PCT) {
        c = LIMITATION_PCT;
    }
    consigne_pct = c;
}

static void calculer_drapeaux(void)
{
    drapeaux = 0u;
    if (limitation_active()) {
        drapeaux |= DRAPEAU_LIMITATION;
    }
    if (touch_enabled) {
        drapeaux |= DRAPEAU_TOUCH;
    }
    if (mode == MODE_AUTONOME && sw_fin) {
        drapeaux |= DRAPEAU_FIN;
    }
    if (feu_actif()) {
        drapeaux |= DRAPEAU_FEU;
    }
    if (obstacle_actif()) {
        drapeaux |= DRAPEAU_OBSTACLE;
    }
    if (defaut_actif()) {
        drapeaux |= DRAPEAU_DEFAUT;
    }
}

void fsm_init(void)
{
    touch_enabled = 0u;
    compteur_transitions = 0u;
    consigne_pot = 0u;
    prochain_sens = 0u;
    clignote_compteur = clignote_phase = 0u;
    expose.vitesse_rpm_q16 = 0u;
    expose.sens = SENS_IMMOBILE;
    mode = MODE_MANUEL;
    cause = CAUSE_AUCUNE;
    plafond_pct = 100u;
    drapeaux = 0u;
    sw_feu = sw_obstacle = sw_limitation = sw_fin = sw_defaut = 0u;
    hum_limite = defauts_i2c = defaut_tp = bip_compteur = 0u;
    (void)motor_set_dir(MOTOR_ARRET);
    etat = ETAT_ARRET;
    pwm_pct = 0u;
    consigne_pct = 0u;
    compteur_transitions = 0u;                  /* fsm_init ne compte pas      */
    fsm_sorties();
}

void fsm_traiter(const evq_event_t *e)
{
    switch (e->type) {
    case EV_ESTOP:
        motor_stop_now();                       /* redondant avec l'ISR        */
        pwm_pct = 0u;
        consigne_pct = 0u;
        buzzer_continu = 1u;
        cause = CAUSE_AUCUNE;
        clignote_compteur = 0u;
        clignote_phase = 1u;
        if (etat != ETAT_ARRET_URGENCE) {
            etat = ETAT_ARRET_URGENCE;
            compteur_transitions++;
        }
        break;

    case EV_USER:
        if (etat == ETAT_ARRET) {
            cause = CAUSE_AUCUNE;
            sw_fin = 0u;
            entree_etat(prochain_sens ? ETAT_MARCHE_ARRIERE : ETAT_MARCHE_AVANT);
            prochain_sens ^= 1u;
        } else if (etat == ETAT_MARCHE_AVANT || etat == ETAT_MARCHE_ARRIERE) {
            entree_etat(ETAT_ARRET);
        }
        /* ARRÊT_URGENCE : User ignoré (E9) */
        break;

    case EV_TOUCH:
        if (etat == ETAT_ARRET_URGENCE) {
            acquitter_urgence();                /* acquittement (E9)           */
        } else if (etat == ETAT_ARRET_EVENEMENT && cause == CAUSE_DEFAUT && defaut_tp) {
            acquitter_evenement();
        } else {
            touch_enabled ^= 1u;                /* laboratoire 1, pour le lab. 4 */
            buzzer_bip(1u);
        }
        break;

    case EV_CONSIGNE:
        consigne_pot = (e->valeur > 100u) ? 100u : e->valeur;   /* suivi en marche (E5)        */
        break;

    case EV_VITESSE:
        expose.vitesse_rpm_q16 = enc_fronts_vers_rpm_q16(e->valeur);  /* E3 */
        expose.sens = sens_mesure;
        break;

    case EV_TOUCHE:
        traiter_touche(e->valeur);
        break;

    case EV_PIR:
        if (en_marche()) {
            uint32_t c = cause_active();

            arret_evenement((c > CAUSE_OBSTACLE) ? c : CAUSE_OBSTACLE);
        }
        break;

    case EV_COMMUT:
        traiter_commutateur(e->valeur);
        break;

    case EV_ENV:
        if (e->valeur > HUM_SEUIL_PCT * 10u) {
            hum_limite = 1u;
        } else if (e->valeur <= (HUM_SEUIL_PCT - HUM_HYSTERESIS_PCT) * 10u) {
            hum_limite = 0u;
        }
        break;

    case EV_DEFAUT:
        traiter_defaut(e->valeur);
        break;

    default:
        break;
    }
}

static void rampe(void)
{
    if (etat != ETAT_MARCHE_AVANT && etat != ETAT_MARCHE_ARRIERE) {
        return;
    }
    if (++rampe_compteur < RAMPE_PAS_MS) {
        return;
    }
    rampe_compteur = 0u;
    if (pwm_pct < consigne_pct) {
        pwm_pct++;
    } else if (pwm_pct > consigne_pct) {
        pwm_pct--;
    } else {
        return;
    }
    (void)motor_set_pwm(pwm_pct);
}

void fsm_sorties(void)
{
    surveiller();
    recalculer_consigne();
    calculer_drapeaux();
    rampe();
    if (etat == ETAT_ARRET_EVENEMENT && cause == CAUSE_OBSTACLE) {
        if (++bip_compteur >= BIP_OBSTACLE_MS) {
            bip_compteur = 0u;
            buzzer_bip(1u);
        }
    } else {
        bip_compteur = 0u;
    }
    if (etat == ETAT_ARRET_URGENCE) {
        if (++clignote_compteur >= CLIGNOTEMENT_DEMI_MS) {
            clignote_compteur = 0u;
            clignote_phase ^= 1u;
        }
        led_set(clignote_phase ? LED_ROUGE : LED_AUCUNE);
    } else if (etat <= ETAT_ARRET_EVENEMENT) {
        led_set(etat_vers_del[etat]);
    }
    buzzer_step();
}

const etat_vehicule_t *fsm_etat(void)
{
    expose.etat = etat;
    expose.consigne_pct = consigne_pct;
    expose.pwm_pct = pwm_pct;
    expose.transitions = compteur_transitions;
    expose.mode = mode;
    expose.cause = cause;
    expose.plafond_pct = plafond_pct;
    expose.drapeaux = drapeaux;
    return &expose;
}
