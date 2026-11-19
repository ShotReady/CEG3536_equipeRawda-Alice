/* ---------------------------------------------------------------------------
 * surv.c — surveillances : sens, encodeur muet, protection thermique
 *          (CEG 3536, projet final, E18, E19) — SQUELETTE À COMPLÉTER
 * Module sans matériel, testé sur hôte : make surv dans hote/.
 *
 * Indications :
 *  - deux compteurs en millisecondes (sens opposé, encodeur muet), augmentés
 *    de dt_ms tant que la condition est vraie, remis à zéro sinon ;
 *  - « plus de 500 ms » : le défaut est levé quand le compteur DÉPASSE le
 *    délai, pas quand il l'atteint ;
 *  - deux comparateurs à hystérésis indépendants pour la température : chacun
 *    garde son état tant que la température est entre ses deux seuils.
 * ------------------------------------------------------------------------- */
#include "surv.h"

static uint32_t code;           /* DEFAUT_x mémorisé                         */
/* À COMPLÉTER : compteurs, états des deux comparateurs thermiques            */
static uint32_t compteur_sens;
static uint32_t compteur_muet;
static uint32_t therm_limitation;
static uint32_t therm_arret;

void surv_init(void)
{
    code = DEFAUT_AUCUN;
    /* À COMPLÉTER */
    compteur_sens = 0u;
    compteur_muet = 0u;
    therm_limitation = 0u;
    therm_arret = 0u;
}

void surv_step(const etat_vehicule_t *v, uint32_t sens_mesure, uint32_t fronts,
               int32_t temp_c10, uint32_t dt_ms)
{
    /* À COMPLÉTER */
    uint32_t en_marche = (v->etat == ETAT_MARCHE_AVANT || v->etat == ETAT_MARCHE_ARRIERE);
    uint32_t oppose = (v->etat == ETAT_MARCHE_AVANT && sens_mesure == SENS_ARRIERE)
                   || (v->etat == ETAT_MARCHE_ARRIERE && sens_mesure == SENS_AVANT);
    uint32_t muet = en_marche && v->pwm_pct > SURV_MUET_PWM_PCT && fronts == 0u;

    compteur_sens = oppose ? compteur_sens + dt_ms : 0u;
    compteur_muet = muet ? compteur_muet + dt_ms : 0u;
    if (code == DEFAUT_AUCUN && compteur_sens > SURV_SENS_MS) {
        code = DEFAUT_SENS;
    }
    if (code == DEFAUT_AUCUN && compteur_muet > SURV_MUET_MS) {
        code = DEFAUT_ENCODEUR;
    }

    if (temp_c10 >= TEMP_LIMITE_C10) {
        therm_limitation = 1u;
    } else if (temp_c10 <= TEMP_LIMITE_FIN_C10) {
        therm_limitation = 0u;
    }
    if (temp_c10 >= TEMP_ARRET_C10) {
        therm_arret = 1u;
    } else if (temp_c10 <= TEMP_ARRET_FIN_C10) {
        therm_arret = 0u;
    }
}

uint32_t surv_code(void)
{
    return code;
}

void surv_acquitter(void)
{
    code = DEFAUT_AUCUN;
    /* À COMPLÉTER : compteurs à zéro */
    compteur_sens = 0u;
    compteur_muet = 0u;
}

uint32_t surv_thermique(void)
{
    /* À COMPLÉTER */
    if (therm_arret) {
        return SURV_THERM_ARRET;
    }
    return therm_limitation ? SURV_THERM_LIMITATION : SURV_THERM_NORMAL;
}
