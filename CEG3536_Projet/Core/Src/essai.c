/* ---------------------------------------------------------------------------
 * essai.c — mode essai : injection de valeurs de capteurs
 *           (CEG 3536, projet final, E20) — SQUELETTE À COMPLÉTER
 * Module sans matériel, testé sur hôte : make surv dans hote/.
 * La bascule du mode (commande m) est fournie ; les injections sont à écrire.
 * ------------------------------------------------------------------------- */
#include "essai.h"

static bool actif;              /* mode essai actif                          */
/* À COMPLÉTER : une variable par injection (température, humidité, sens,
 * encodeur muet), chacune avec son indicateur « injection en cours »         */
static bool    temp_injectee;
static int32_t temp_valeur;
static bool    hum_injectee;
static uint32_t hum_valeur;
static bool    sens_inverse;
static bool    encodeur_muet;

static void annuler_injections(void)
{
    /* À COMPLÉTER */
    temp_injectee = false;
    temp_valeur = 0;
    hum_injectee = false;
    hum_valeur = 0u;
    sens_inverse = false;
    encodeur_muet = false;
}

void essai_init(void)
{
    actif = false;
    annuler_injections();
}

bool essai_actif(void)
{
    return actif;
}

void essai_basculer(void)
{
    actif = !actif;
    if (!actif) {
        annuler_injections();           /* retour aux mesures réelles        */
    }
}

bool essai_commande(char c)
{
    if (c == 'm') {
        essai_basculer();
        return true;
    }
    if (!actif) {
        return false;                   /* injections refusées hors du mode  */
    }
    /* À COMPLÉTER : 1, 2, 3, h, s, n */
    switch (c) {
    case '1':
        temp_injectee = true;
        temp_valeur = ESSAI_TEMP_1_C10;
        return true;
    case '2':
        temp_injectee = true;
        temp_valeur = ESSAI_TEMP_2_C10;
        return true;
    case '3':
        temp_injectee = true;
        temp_valeur = ESSAI_TEMP_3_C10;
        return true;
    case 'h':
        hum_valeur = (hum_injectee && hum_valeur == ESSAI_HUM_HAUTE_X10)
                     ? ESSAI_HUM_BASSE_X10 : ESSAI_HUM_HAUTE_X10;
        hum_injectee = true;
        return true;
    case 's':
        sens_inverse = !sens_inverse;
        return true;
    case 'n':
        encodeur_muet = !encodeur_muet;
        return true;
    default:
        return false;
    }
}

int32_t essai_temp_c10(int32_t mesure)
{
    /* À COMPLÉTER */
    return (actif && temp_injectee) ? temp_valeur : mesure;
}

uint32_t essai_hum_x10(uint32_t mesure)
{
    /* À COMPLÉTER */
    return (actif && hum_injectee) ? hum_valeur : mesure;
}

uint32_t essai_sens(uint32_t mesure)
{
    /* À COMPLÉTER */
    if (actif && sens_inverse) {
        if (mesure == SENS_AVANT) {
            return SENS_ARRIERE;
        }
        if (mesure == SENS_ARRIERE) {
            return SENS_AVANT;
        }
    }
    return mesure;
}

uint32_t essai_fronts(uint32_t mesure)
{
    /* À COMPLÉTER */
    return (actif && encodeur_muet) ? 0u : mesure;
}
