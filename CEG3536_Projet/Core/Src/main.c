/* ---------------------------------------------------------------------------
 * main.c — programme principal en C (CEG 3536, laboratoire 3) — CORRIGÉ
 *
 * Remplace main.s : initialise la couche assembleur (laboratoires 1 et 2),
 * puis les pilotes C (ADC/DMA, LPUART1, DWT), et exécute une boucle cadencée
 * par tick_1ms (E1, E10) :
 *   production d'événements (boutons, E-Stop, encodeur, consigne, télémétrie)
 *   -> consommation par la machine à états -> sorties -> émission série.
 * Commandes série facultatives (un caractère) : z = remise à zéro des
 * compteurs ; t = 40 événements de test (saturation de la file, E12) ;
 * q = autotest de la file (E11) ; b = banc de comparaison L3-A (en ARRÊT).
 * ------------------------------------------------------------------------- */
#include "stm32l552xx.h"
#include "lab3.h"
#include "evq.h"
#include "evq_test.h"
#include "acq.h"
#include "adc_dma.h"
#include "telem.h"
#include "fsm.h"
#include "bench_l3a.h"
#include "lab4.h"
#include "i2c.h"
#include "spi.h"
#include "ioexp.h"
#include "lcd.h"
#include "touchpad.h"
#include "env.h"
#include "tableau.h"
#include "tableau_test.h"
#include "somme_test.h"
#include "projet.h"
#include "trajet.h"
#include "regul.h"
#include "surv.h"
#include "essai.h"
#include "crit_test.h"
#include "automate_gen.h"

#define TP_RELIRE_MS        50u
#define COMMUT_N_VALIDATION (COMMUT_ANTIREBOND_MS / COMMUT_PERIODE_MS)
#define BIT_LCD             (1u << 0)
#define BIT_DEL             (1u << 1)
#define BIT_COMMUT          (1u << 2)
#define BIT_TOUCHPAD        (1u << 3)
#define INIT_ATTENTE_MS     300u
#define ENV_ID_ATTENDU      0x61u
#define TEMP_PLAUSIBLE_MIN  (-300)
#define TEMP_PLAUSIBLE_MAX  1200

/* tampon DMA : ADC_N_MOY échantillons par voie, entrelacés IN1, IN2, ... */
static volatile uint16_t tampon_adc[ADC_N_VOIES * ADC_N_MOY];
static acq_t acq_pot;      /* potentiomètre : 0..100 %                        */
static acq_t acq_temp;     /* température : dixièmes de degré Celsius         */

volatile uint32_t compteur_pas;          /* passages dans la boucle (watch)    */
volatile uint32_t ticks_perdus;
volatile uint32_t touches_refusees;

static ioexp_t      del_exp, commut_exp;
static env_mesure_t env_cour;
static uint16_t     tp_etat;
static uint32_t     tp_echecs, tp_t_echec, tp_a_relire;
static uint32_t     commut_t, defauts_signales;
static uint8_t      commut_compteur[8];
static regul_t      regul;
static uint32_t     trajet_num = 1u;

void SystemInit(void)
{
    /* horloge MSI 4 MHz de la réinitialisation : rien à faire (HORLOGE_HZ_C) */
}

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void reponse_u32(const char *etiquette, uint32_t v)
{
    char ligne[48];
    char *p = ligne;
    while (*etiquette) *p++ = *etiquette++;
    p = telem_fmt_u32(p, v);
    *p++ = '\r'; *p++ = '\n'; *p = '\0';
    telem_put(ligne);
}

static void reponse_autotest_auto(void)
{
    static const char chiffres[] = "0123456789ABCDEF";
    char ligne[64];
    char *p = ligne;
    const char *s = "G,echecs=";
    uint32_t ecarts = automate_autotest();

    while (*s) *p++ = *s++;
    p = telem_fmt_u32(p, ecarts);
    s = ",somme=";
    while (*s) *p++ = *s++;
    for (int32_t i = 28; i >= 0; i -= 4) {
        *p++ = chiffres[(AUTO_SOMME >> i) & 0xFu];
    }
    *p++ = '\r'; *p++ = '\n'; *p = '\0';
    telem_put(ligne);
}

static uint32_t verifier_ressources(const uint8_t *adresses, uint32_t n, uint32_t fronts)
{
    static const uint8_t requises[5] = { I2C_ADR_RVB, I2C_ADR_DEL, I2C_ADR_COMMUT,
                                         I2C_ADR_LCD, I2C_ADR_TOUCHPAD };
    int32_t temp = acq_valeur(&acq_temp);

    for (uint32_t r = 0u; r < 5u; r++) {
        uint32_t present = 0u;

        for (uint32_t i = 0u; i < n; i++) {
            if (adresses[i] == requises[r]) {
                present = 1u;
            }
        }
        if (!present) {
            return DEFAUT_INIT;
        }
    }
    if (env_identifiant() != ENV_ID_ATTENDU) {
        return DEFAUT_INIT;
    }
    if (acq_mv(&acq_pot) > ADC_VREF_MV + 100u || temp < TEMP_PLAUSIBLE_MIN || temp > TEMP_PLAUSIBLE_MAX) {
        return DEFAUT_INIT;
    }
    if (fronts != 0u) {
        return DEFAUT_INIT;
    }
    return DEFAUT_AUCUN;
}

static void suivre_defaut(uint32_t bit, uint8_t adr, uint32_t echecs, uint32_t t)
{
    if (echecs >= I2C_ERREURS_DEFAUT) {
        if ((defauts_signales & bit) == 0u) {
            defauts_signales |= bit;
            evq_push(EV_DEFAUT, adr, t);
        }
    } else if (echecs == 0u && (defauts_signales & bit) != 0u) {
        defauts_signales &= ~bit;
        evq_push(EV_DEFAUT, (uint32_t)adr | DEFAUT_I2C_RETABLI, t);
    }
}

static void suivre_defauts(uint32_t t)
{
    suivre_defaut(BIT_LCD, I2C_ADR_LCD, lcd_echecs(), t);
    suivre_defaut(BIT_DEL, I2C_ADR_DEL, del_exp.echecs, t);
    suivre_defaut(BIT_COMMUT, I2C_ADR_COMMUT, commut_exp.echecs, t);
    suivre_defaut(BIT_TOUCHPAD, I2C_ADR_TOUCHPAD, tp_echecs, t);
}

static void commutateurs_valider(uint8_t octet, uint32_t t)
{
    for (uint32_t i = 0u; i < 8u; i++) {
        if ((octet & (1u << i)) == 0u) {
            if (commut_compteur[i] < COMMUT_N_VALIDATION) {
                commut_compteur[i]++;
                if (commut_compteur[i] == COMMUT_N_VALIDATION) {
                    evq_push(EV_COMMUT, i + 1u, t);
                }
            }
        } else {
            commut_compteur[i] = 0u;
        }
    }
}

static void bus_tick(uint32_t t)
{
    uint8_t octet;
    uint8_t voulu;

    if (thi_flag || (tp_a_relire && (t - tp_t_echec) >= TP_RELIRE_MS)) {
        uint16_t etat12;

        thi_flag = 0u;
        tp_a_relire = 0u;
        if (tp_lire(&etat12)) {
            int32_t k = tp_touche(tp_etat, etat12);

            tp_echecs = 0u;
            tp_etat = etat12;
            if (k >= 0) {
                if (touch_enabled) {
                    evq_push(EV_TOUCHE, (uint32_t)k, t);
                } else {
                    touches_refusees++;
                    tableau_verrou();
                }
            }
        } else {
            tp_echecs++;
            tp_a_relire = 1u;
            tp_t_echec = t;
        }
        return;
    }
    if ((t - commut_t) >= COMMUT_PERIODE_MS) {
        commut_t = t;
        if (ioexp_lire(&commut_exp, &octet)) {
            commutateurs_valider(octet, t);
        }
        return;
    }
    voulu = (uint8_t)~tableau_image_del();
    if (!del_exp.sortie_connue || del_exp.sortie != voulu) {
        ioexp_ecrire(&del_exp, voulu);
        return;
    }
    lcd_step();
}

int main(void)
{
    uint32_t fenetre = 0u, periode_telem = 0u;
    uint32_t dernier_tick;
    uint8_t  adresses[16];
    uint32_t n_adresses;
    int32_t  derniere_consigne = -1;
    evq_event_t e;
    uint32_t init_fait = 0u, auto_prec = 0u, fin_signale = 0u;
    uint32_t code_prec = DEFAUT_AUCUN, therm_prec = SURV_THERM_NORMAL, deb_prec = 0u;

    /* couche assembleur (sorties en état sûr avant les interruptions) */
    gpio_init();
    buzzer_init();
    motor_init();
    encoder_init();
    estop_init();
    systick_init(1000u);
    /* couche C */
    dwt_init();
    evq_init();
    crit_rafale_init();
    essai_init();
    surv_init();
    trajet_init(TRAJET_REF, TRAJET_REF_N);
    regul_init(&regul, REGUL_KP_Q8, REGUL_KI_Q8);
    adc_dma_init(tampon_adc, ADC_N_MOY);
    /* potentiomètre : % = mv x 100 / 3300 ; version 2 : zone morte 3 %, hystérésis 2 */
    acq_init(&acq_pot, &tampon_adc[0], ADC_N_VOIES, ADC_N_MOY, 100, (int32_t)ADC_VREF_MV, 0);
    acq_config_filtre(&acq_pot, CONSIGNE_HYSTERESIS, POT_ZONE_MORTE_PCT, 0, 100);
    /* température : t10 = (mv - MV_REF) x (-10900) / 1000 ... en dixièmes :
     * t10 = mv x (-10000) / 10900 + (C10_REF + MV_REF x 10000 / 10900)        */
    acq_init(&acq_temp, &tampon_adc[1], ADC_N_VOIES, ADC_N_MOY,
             -10000, -TEMP_PENTE_UV_PAR_C,
             TEMP_C10_REF + (TEMP_MV_REF * 10000) / (-TEMP_PENTE_UV_PAR_C));
    telem_init(TELEM_BAUDS);
    fsm_init();
    telem_entete();
    i2c_init();
    spi_init();
    n_adresses = i2c_scan(adresses, sizeof adresses);
    telem_balayage(adresses, n_adresses);
    ioexp_init(&del_exp, I2C_ADR_DEL);
    ioexp_init(&commut_exp, I2C_ADR_COMMUT);
    ioexp_ecrire(&commut_exp, 0xFFu);
    if (tp_init()) {
        uint16_t etat12;

        if (tp_lire(&etat12)) {
            tp_etat = etat12;
        }
    } else {
        tp_echecs = I2C_ERREURS_DEFAUT;
    }
    lcd_init();
    (void)env_init();
    tableau_init();
    exti_l4_init();
    dernier_tick = ticks_ms;
    commut_t = ticks_ms;

    for (;;) {
        if (tick_1ms == 0u) {
            continue;                                   /* attente du tick    */
        }
        tick_1ms = 0u;
        compteur_pas++;
        uint32_t t = ticks_ms;

        if ((t - dernier_tick) > 1u) {
            ticks_perdus += (t - dernier_tick) - 1u;
        }
        dernier_tick = t;

        /* --- producteurs d'événements --- */
        if (button_pressed(BTN_USER))  { evq_push(EV_USER, 0u, t); }
        if (button_pressed(BTN_TOUCH)) { evq_push(EV_TOUCH, 0u, t); }
        if (estop_flag) {
            estop_flag = 0u;
            motor_stop_now();                           /* redondant avec l'ISR */
            evq_push(EV_ESTOP, 0u, t);
        }
        if (pir_flag) {
            pir_flag = 0u;
            evq_push(EV_PIR, 1u, t);
        }
        if (++fenetre >= FENETRE_VITESSE_MS) {
            fenetre = 0u;
            uint32_t fronts = encoder_read(&sens_mesure);
            evq_push(EV_VITESSE, fronts, t);
            if (PARTIE_II) {
                uint32_t rpm = (enc_fronts_vers_rpm_q16(fronts) + 32768u) >> 16;
                uint32_t autonome = fsm_autonome_actif() ? 1u : 0u;
                uint32_t consigne_rpm, plafond_pwm, pwm, code, niveau;

                if (autonome && !auto_prec) {
                    if (trajet_fini()) {
                        trajet_reset();
                    }
                    fin_signale = 0u;
                }
                auto_prec = autonome;
                if (autonome) {
                    trajet_avancer(fronts);
                    consigne_rpm = fsm_regulation(trajet_consigne_rpm(), &plafond_pwm);
                    pwm = regul_step(&regul, consigne_rpm, rpm, plafond_pwm);
                    fsm_pwm_regule(pwm);
                    if (trajet_fini() && !fin_signale) {
                        fin_signale = 1u;
                        evq_push(EV_FIN, 0u, t);
                    }
                } else {
                    regul_reset(&regul);
                }
                fsm_trajet_exposer(trajet_segment(), trajet_dist_x10());
                surv_step(fsm_etat(), essai_sens(sens_mesure), essai_fronts(fronts),
                          essai_temp_c10(acq_valeur(&acq_temp)), FENETRE_VITESSE_MS);
                code = surv_code();
                if (code != code_prec) {
                    code_prec = code;
                    if (code != DEFAUT_AUCUN) {
                        evq_push(EV_DEFAUT, EV_DEFAUT_CODE_BASE + code, t);
                    }
                }
                niveau = surv_thermique();
                if (niveau != therm_prec) {
                    therm_prec = niveau;
                    evq_push(EV_THERMIQUE, niveau, t);
                }
            }
        }
        int32_t c = acq_valeur(&acq_pot);
        if (c != derniere_consigne) {
            derniere_consigne = c;
            evq_push(EV_CONSIGNE, (uint32_t)c, t);
        }
        if (++periode_telem >= TELEM_PERIODE_MS) {
            periode_telem = 0u;
            evq_push(EV_TELEM, 0u, t);
        }
        adc_dma_step();
        env_step();
        if (env_mesure(&env_cour)) {
            env_cour.h_x10 = essai_hum_x10(env_cour.h_x10);
            evq_push(EV_ENV, env_cour.h_x10, t);
        }
        env_cour.ta_c10 = essai_temp_c10(acq_valeur(&acq_temp));
        tableau_env(&env_cour);

        if (PARTIE_II && !init_fait && t >= INIT_ATTENTE_MS) {
            uint32_t code_init = verifier_ressources(adresses, n_adresses, encoder_read(&sens_mesure));

            init_fait = 1u;
            if (code_init == DEFAUT_AUCUN) {
                fsm_init_fin(DEFAUT_AUCUN);
            } else {
                evq_push(EV_DEFAUT, EV_DEFAUT_CODE_BASE + code_init, t);
            }
        }
        if (evq_debordements() != deb_prec) {
            deb_prec = evq_debordements();
            fsm_message_file();
        }

        /* --- commandes série facultatives --- */
        int rx = telem_rx();
        if (rx == 'z') {
            evq_init();
            deb_prec = 0u;
            derniere_consigne = -1;
            telem_put("Z,ok\r\n");
        } else if (rx == 't') {
            for (uint32_t i = 0u; i < 40u; i++) { evq_push(EV_TEST, i, t); }
        } else if (rx == 'q') {
            reponse_u32("Q,echecs=", evq_autotest());
        } else if (rx == 'b') {
            if (etat == ETAT_ARRET) {
                reponse_u32("B,ecarts=", bench_l3a());
                reponse_u32("B,cycles_asm=", bench_cycles_asm);
                reponse_u32("B,cycles_c=", bench_cycles_c);
            } else {
                telem_put("B,refuse (ARRET requis)\r\n");
            }
        } else if (rx == 'd') {
            i2c_simuler_absence(I2C_ADR_LCD, ABSENCE_SIMULEE_MS);
            telem_put("D,ok\r\n");
        } else if (rx == 'x') {
            reponse_u32("X,echecs=", somme_autotest());
        } else if (rx == 'i') {
            n_adresses = i2c_scan(adresses, sizeof adresses);
            telem_balayage(adresses, n_adresses);
        } else if (rx == 'a') {
            reponse_u32("A,pot_brut=", acq_brut(&acq_pot));
            reponse_u32("A,pot_mv=", acq_mv(&acq_pot));
            reponse_u32("A,temp_brut=", acq_brut(&acq_temp));
            reponse_u32("A,temp_mv=", acq_mv(&acq_temp));
            reponse_u32("A,tours_dma=", adc_dma_tours());
        } else if (rx == 'e') {
            crit_rafale(ESSAI_RAFALE_N);
            telem_put("E,rafale\r\n");
        } else if (rx == 'r') {
            if (etat == ETAT_ARRET) {
                trajet_reset();
                telem_put("R,ok\r\n");
            } else {
                telem_put("R,refuse (ARRET requis)\r\n");
            }
        } else if (rx == 'j') {
            if (etat == ETAT_ARRET) {
                trajet_num = (trajet_num == 1u) ? 2u : 1u;
                if (trajet_num == 1u) {
                    trajet_init(TRAJET_REF, TRAJET_REF_N);
                } else {
                    trajet_init(TRAJET_2, TRAJET_2_N);
                }
                reponse_u32("J,trajet=", trajet_num);
            } else {
                telem_put("J,refuse (ARRET requis)\r\n");
            }
        } else if (rx == 'c') {
            reponse_u32("C,echecs=", crit_autotest());
        } else if (rx == 'g') {
            reponse_autotest_auto();
        } else if (rx == 'f') {
            reponse_u32("F,echecs=", tableau_autotest());
        } else if (rx == 'm' || rx == '1' || rx == '2' || rx == '3'
                   || rx == 'h' || rx == 's' || rx == 'n') {
            if (essai_commande((char)rx)) {
                fsm_essai(essai_actif() ? 1u : 0u);
                evq_push(EV_ESSAI, (uint32_t)rx, t);
            } else {
                telem_put("M,refuse (mode essai requis)\r\n");
            }
        }

        /* --- consommateur : machine à états --- */
        while (evq_pop(&e)) {
            if (e.type == EV_TELEM) {
                telem_trame(fsm_etat(), essai_temp_c10(acq_valeur(&acq_temp)), evq_total(), evq_debordements(),
                            &env_cour, i2c_erreurs(), ticks_perdus);
            } else {
                if (e.type == EV_PIR || e.type == EV_TOUCHE || e.type == EV_COMMUT
                    || e.type == EV_ESTOP || e.type == EV_DEFAUT
                    || e.type == EV_THERMIQUE || e.type == EV_FIN || e.type == EV_ESSAI) {
                    telem_evenement(&e);
                }
                if (e.type == EV_COMMUT && e.valeur == SW_PAGE) {
                    tableau_page_basculer();
                }
                if (PARTIE_II && e.type == EV_COMMUT && e.valeur == SW_TRAJET && etat == ETAT_ARRET) {
                    trajet_reset();
                }
                if (PARTIE_II && e.type == EV_COMMUT && e.valeur == SW_ESSAI) {
                    essai_basculer();
                    fsm_essai(essai_actif() ? 1u : 0u);
                }
                fsm_traiter(&e);
            }
        }
        fsm_sorties();
        tableau_step();
        bus_tick(t);
        suivre_defauts(t);
        telem_step();
    }
}
