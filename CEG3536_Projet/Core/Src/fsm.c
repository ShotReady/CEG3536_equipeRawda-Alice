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
#include "projet.h"
#include "automate_gen.h"
#include "surv.h"
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

static const uint8_t etat_vers_del[7]  = { LED_ROUGE, LED_VERTE, LED_BLEUE, LED_ROUGE, LED_ROUGE, LED_ROUGE, LED_ROUGE };
static const uint8_t etat_vers_dir[7]  = { MOTOR_ARRET, MOTOR_AVANT, MOTOR_ARRIERE, MOTOR_ARRET, MOTOR_ARRET, MOTOR_ARRET, MOTOR_ARRET };
static const uint8_t etat_vers_bips[7] = { 1u, 1u, 2u, 0u, 0u, 0u, 0u };

#define PIR_BROCHE      9u

static uint32_t mode, cause, plafond_pct, drapeaux;
static uint32_t sw_feu, sw_obstacle, sw_limitation, sw_fin, sw_defaut;
static uint32_t hum_limite, defauts_i2c, defaut_tp, bip_compteur;
static auto_etat_t as;
static uint32_t therm_niveau, fin_trajet, defaut_surv, defaut_init;
static uint32_t consigne_auto_rpm, consigne_auto_pct, traj_segment, traj_dist_x10;
static uint32_t essai_actif, file_actif, file_debut;

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

static uint32_t defaut_vivant(void)
{
    return (mode == MODE_AUTONOME && sw_defaut) || defauts_i2c != 0u;
}

static uint32_t defaut_actif(void)
{
    return defaut_vivant() || defaut_surv != 0u || defaut_init != 0u;
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
    return hum_limite || (mode == MODE_AUTONOME && sw_limitation)
        || (PARTIE_II && therm_niveau >= SURV_THERM_LIMITATION);
}

static uint32_t cause_active(void)
{
    if (defaut_actif()) {
        return CAUSE_DEFAUT;
    }
    if (PARTIE_II && therm_niveau == SURV_THERM_ARRET) {
        return CAUSE_THERMIQUE;
    }
    if (obstacle_actif()) {
        return CAUSE_OBSTACLE;
    }
    if (feu_actif()) {
        return CAUSE_FEU;
    }
    return CAUSE_AUCUNE;
}

static uint32_t etat_de(auto_etat_t a)
{
    switch (a) {
    case AUTO_ETAT_INIT:
        return ETAT_INIT;
    case AUTO_ETAT_MANUEL_AV:
    case AUTO_ETAT_AUTO_AV:
        return ETAT_MARCHE_AVANT;
    case AUTO_ETAT_MANUEL_AR:
    case AUTO_ETAT_AUTO_AR:
        return ETAT_MARCHE_ARRIERE;
    case AUTO_ETAT_ARRET_OBSTACLE:
    case AUTO_ETAT_ARRET_FEU:
    case AUTO_ETAT_ARRET_THERMIQUE:
        return ETAT_ARRET_EVENEMENT;
    case AUTO_ETAT_URGENCE:
        return ETAT_ARRET_URGENCE;
    case AUTO_ETAT_DEFAUT:
        return PARTIE_II ? ETAT_DEFAUT : ETAT_ARRET_EVENEMENT;
    default:
        return ETAT_ARRET;
    }
}

static uint32_t cause_de(auto_etat_t a)
{
    switch (a) {
    case AUTO_ETAT_ARRET_OBSTACLE: return CAUSE_OBSTACLE;
    case AUTO_ETAT_ARRET_FEU:      return CAUSE_FEU;
    case AUTO_ETAT_ARRET_THERMIQUE: return CAUSE_THERMIQUE;
    case AUTO_ETAT_DEFAUT:         return CAUSE_DEFAUT;
    default:                       return CAUSE_AUCUNE;
    }
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
    entree_etat(etat_de(as));
    if (c != CAUSE_FEU) {
        buzzer_bip(1u);
    }
}

static void appliquer(auto_entree_t x)
{
    auto_etat_t prec = as;
    auto_etat_t suiv = automate_transition(prec, x, 0);

    if (suiv == prec) {
        return;
    }
    as = suiv;
    switch (suiv) {
    case AUTO_ETAT_PRET:
        if (x == AUTO_ENTREE_ACQUIT && prec == AUTO_ETAT_DEFAUT) {
            surv_acquitter();
            defaut_surv = 0u;
            defaut_init = 0u;
        }
        buzzer_continu = 0u;
        cause = (x == AUTO_ENTREE_FIN_TRAJET) ? CAUSE_FIN : CAUSE_AUCUNE;
        defaut_tp = 0u;
        entree_etat(ETAT_ARRET);
        break;
    case AUTO_ETAT_MANUEL_AV:
    case AUTO_ETAT_MANUEL_AR:
    case AUTO_ETAT_AUTO_AV:
    case AUTO_ETAT_AUTO_AR:
        cause = CAUSE_AUCUNE;
        sw_fin = 0u;
        fin_trajet = 0u;
        entree_etat(etat_de(suiv));
        prochain_sens ^= 1u;
        break;
    case AUTO_ETAT_ARRET_OBSTACLE:
    case AUTO_ETAT_ARRET_FEU:
    case AUTO_ETAT_ARRET_THERMIQUE:
    case AUTO_ETAT_DEFAUT:
        if (etat_de(prec) == etat_de(suiv)) {
            cause = cause_de(suiv);
        } else {
            arret_evenement(cause_de(suiv));
        }
        break;
    case AUTO_ETAT_URGENCE:
        etat = ETAT_ARRET_URGENCE;
        compteur_transitions++;
        break;
    default:
        break;
    }
}

static uint32_t acquit_permis(void)
{
    switch (as) {
    case AUTO_ETAT_URGENCE:
        return button_raw(BTN_ESTOP) == 0u;
    case AUTO_ETAT_DEFAUT:
        return !defaut_vivant();
    case AUTO_ETAT_ARRET_OBSTACLE:
    case AUTO_ETAT_ARRET_FEU:
    case AUTO_ETAT_ARRET_THERMIQUE:
        return cause_active() == CAUSE_AUCUNE;
    default:
        return 0u;
    }
}

static void acquitter(void)
{
    if (acquit_permis()) {
        appliquer(AUTO_ENTREE_ACQUIT);
    }
}

static uint32_t niveaux(void)
{
    uint32_t masque = 0u;

    if (defaut_actif()) {
        masque |= 1u << AUTO_ENTREE_PANNE;
    }
    if (PARTIE_II && therm_niveau == SURV_THERM_ARRET) {
        masque |= 1u << AUTO_ENTREE_THERMIQUE;
    }
    if (obstacle_actif()) {
        masque |= 1u << AUTO_ENTREE_OBSTACLE;
    }
    if (feu_actif()) {
        masque |= 1u << AUTO_ENTREE_FEU;
    }
    if ((mode == MODE_AUTONOME && sw_fin) || fin_trajet) {
        masque |= 1u << AUTO_ENTREE_FIN_TRAJET;
    }
    return masque;
}

static auto_entree_t demande_depart(void)
{
    if (mode == MODE_AUTONOME) {
        return prochain_sens ? AUTO_ENTREE_DEM_AUTO_AR : AUTO_ENTREE_DEM_AUTO_AV;
    }
    return prochain_sens ? AUTO_ENTREE_DEM_MAN_AR : AUTO_ENTREE_DEM_MAN_AV;
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
        acquitter();
        buzzer_bip(1u);
    } else if (touche == (uint32_t)TOUCHE_NEXT) {
        if (as == AUTO_ETAT_PRET) {
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
    uint32_t bit;

    if (valeur >= EV_DEFAUT_CODE_BASE) {
        uint32_t code = valeur - EV_DEFAUT_CODE_BASE;

        if (code == DEFAUT_SENS || code == DEFAUT_ENCODEUR) {
            defaut_surv = code;
        } else if (code == DEFAUT_INIT) {
            defaut_init = code;
        }
        return;
    }
    bit = bit_module(valeur & ~DEFAUT_I2C_RETABLI);
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
    auto_entree_t x = automate_choisir(niveaux());

    if (x != AUTO_N_ENTREES) {
        appliquer(x);
    }
}

static void recalculer_consigne(void)
{
    uint32_t c;

    if (!en_marche()) {
        consigne_pct = 0u;
        return;
    }
    if (PARTIE_II && mode == MODE_AUTONOME) {
        c = consigne_auto_pct;
    } else {
        c = (mode == MODE_AUTONOME) ? CONSIGNE_AUTO_PCT : consigne_pot;
    }
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
    if (mode == MODE_AUTONOME && (sw_fin || fin_trajet)) {
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
    if (PARTIE_II && therm_niveau == SURV_THERM_ARRET) {
        drapeaux |= DRAPEAU_THERMIQUE;
    }
    if (essai_actif) {
        drapeaux |= DRAPEAU_ESSAI;
    }
    if (file_actif && (ticks_ms - file_debut) < FILE_MESSAGE_MS) {
        drapeaux |= DRAPEAU_FILE;
    } else {
        file_actif = 0u;
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
    therm_niveau = fin_trajet = defaut_surv = defaut_init = 0u;
    consigne_auto_rpm = consigne_auto_pct = traj_segment = traj_dist_x10 = 0u;
    essai_actif = file_actif = file_debut = 0u;
    (void)motor_set_dir(MOTOR_ARRET);
    as = AUTO_ETAT_INITIAL;
    etat = ETAT_INIT;
    if (!PARTIE_II) {
        as = automate_transition(as, AUTO_ENTREE_INIT_OK, 0);
        etat = ETAT_ARRET;
    }
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
        appliquer(AUTO_ENTREE_ESTOP);
        break;

    case EV_USER:
        if (as == AUTO_ETAT_PRET) {
            appliquer(demande_depart());
        } else if (en_marche()) {
            appliquer(AUTO_ENTREE_USER_ARRET);
        }
        /* ARRÊT_URGENCE : User ignoré (E9) */
        break;

    case EV_TOUCH:
        if (as == AUTO_ETAT_URGENCE) {
            acquitter();                        /* acquittement (E9)           */
        } else if (as == AUTO_ETAT_DEFAUT && defaut_tp) {
            acquitter();
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
            appliquer(automate_choisir(niveaux() | (1u << AUTO_ENTREE_OBSTACLE)));
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

    case EV_THERMIQUE:
        therm_niveau = e->valeur;
        break;

    case EV_FIN:
        if (mode == MODE_AUTONOME && en_marche()) {
            fin_trajet = 1u;
        }
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
    if (PARTIE_II && mode == MODE_AUTONOME) {
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
    if (etat == ETAT_ARRET_EVENEMENT && (cause == CAUSE_OBSTACLE || cause == CAUSE_THERMIQUE)) {
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
    } else if (etat <= ETAT_DEFAUT) {
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
    expose.consigne_rpm = fsm_autonome_actif() ? consigne_auto_rpm : 0u;
    expose.segment = (mode == MODE_AUTONOME) ? traj_segment : 0u;
    expose.dist_x10 = traj_dist_x10;
    expose.mode_essai = essai_actif;
    if (defaut_init != 0u) {
        expose.code_defaut = DEFAUT_INIT;
    } else if (defaut_surv != 0u) {
        expose.code_defaut = defaut_surv;
    } else if (defauts_i2c != 0u) {
        expose.code_defaut = DEFAUT_I2C;
    } else if (mode == MODE_AUTONOME && sw_defaut) {
        expose.code_defaut = DEFAUT_SIMULE;
    } else {
        expose.code_defaut = DEFAUT_AUCUN;
    }
    return &expose;
}

void fsm_init_fin(uint32_t code_defaut)
{
    if (as != AUTO_ETAT_INIT) {
        return;
    }
    if (code_defaut == DEFAUT_AUCUN) {
        appliquer(AUTO_ENTREE_INIT_OK);
    } else {
        defaut_init = code_defaut;
        appliquer(AUTO_ENTREE_PANNE);
    }
}

uint32_t fsm_regulation(uint32_t consigne_trajet_rpm, uint32_t *plafond_pwm_pct)
{
    uint32_t c = consigne_trajet_rpm;
    uint32_t borne = plafond_pct * VITESSE_MAX_RPM / 100u;

    if (c > borne) {
        c = borne;
    }
    if (limitation_active()) {
        borne = LIMITATION_PCT * VITESSE_MAX_RPM / 100u;
        if (c > borne) {
            c = borne;
        }
    }
    consigne_auto_rpm = c;
    consigne_auto_pct = c * 100u / VITESSE_MAX_RPM;
    if (plafond_pwm_pct != 0) {
        *plafond_pwm_pct = plafond_pct;
    }
    return c;
}

void fsm_pwm_regule(uint32_t pwm_pct_voulu)
{
    if (PARTIE_II && fsm_autonome_actif()) {
        pwm_pct = (pwm_pct_voulu > 100u) ? 100u : pwm_pct_voulu;
        (void)motor_set_pwm(pwm_pct);
    }
}

bool fsm_autonome_actif(void)
{
    return as == AUTO_ETAT_AUTO_AV || as == AUTO_ETAT_AUTO_AR;
}

void fsm_trajet_exposer(uint32_t segment, uint32_t dist_x10)
{
    traj_segment = segment;
    traj_dist_x10 = dist_x10;
}

void fsm_essai(uint32_t actif)
{
    essai_actif = actif;
}

void fsm_message_file(void)
{
    file_actif = 1u;
    file_debut = ticks_ms;
}
