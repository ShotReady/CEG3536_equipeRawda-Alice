/* ---------------------------------------------------------------------------
 * automate_gen.c — GENERE par outils/automate.py 1.0 a partir de automate.json
 * Automate « vehicule », somme de la table : 43985C1A
 * NE PAS MODIFIER A LA MAIN : corriger la table JSON, verifier, puis regenerer.
 * ------------------------------------------------------------------------- */
#include "automate_gen.h"

/* table[etat][entree] = etat suivant */
static const uint8_t table[AUTO_N_ETATS][AUTO_N_ENTREES] = {
    /* INIT               */ {   9,  10,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1 },
    /* PRET               */ {   9,  10,   1,   1,   1,   1,   1,   1,   2,   3,   4,   5,   1 },
    /* MANUEL_AV          */ {   9,  10,   8,   6,   2,   2,   2,   1,   2,   2,   2,   2,   2 },
    /* MANUEL_AR          */ {   9,  10,   8,   6,   3,   3,   3,   1,   3,   3,   3,   3,   3 },
    /* AUTO_AV            */ {   9,  10,   8,   6,   7,   1,   4,   1,   4,   4,   4,   4,   4 },
    /* AUTO_AR            */ {   9,  10,   8,   6,   7,   1,   5,   1,   5,   5,   5,   5,   5 },
    /* ARRET_OBSTACLE     */ {   9,  10,   8,   6,   6,   6,   1,   6,   6,   6,   6,   6,   6 },
    /* ARRET_FEU          */ {   9,  10,   8,   6,   7,   7,   1,   7,   7,   7,   7,   7,   7 },
    /* ARRET_THERMIQUE    */ {   9,  10,   8,   8,   8,   8,   1,   8,   8,   8,   8,   8,   8 },
    /* URGENCE            */ {   9,   9,   9,   9,   9,   9,   1,   9,   9,   9,   9,   9,   9 },
    /* DEFAUT             */ {   9,  10,  10,  10,  10,  10,   1,  10,  10,  10,  10,  10,  10 },
};

static const auto_sorties_t sorties[AUTO_N_ETATS] = {
    /* INIT               */ { 0, 0, 0, 0, 0, 0, 5 },
    /* PRET               */ { 0, 0, 0, 0, 0, 0, 0 },
    /* MANUEL_AV          */ { 1, 1, 0, 1, 0, 0, 1 },
    /* MANUEL_AR          */ { 1, 2, 0, 2, 0, 0, 2 },
    /* AUTO_AV            */ { 1, 1, 1, 1, 0, 0, 1 },
    /* AUTO_AR            */ { 1, 2, 1, 2, 0, 0, 2 },
    /* ARRET_OBSTACLE     */ { 0, 0, 0, 0, 3, 2, 4 },
    /* ARRET_FEU          */ { 0, 0, 0, 0, 2, 0, 4 },
    /* ARRET_THERMIQUE    */ { 0, 0, 0, 0, 5, 2, 4 },
    /* URGENCE            */ { 0, 0, 0, 3, 0, 1, 3 },
    /* DEFAUT             */ { 0, 0, 0, 0, 4, 3, 6 },
};

static const char *const noms_etats[AUTO_N_ETATS] = {
    "INIT", "PRET", "MANUEL_AV", "MANUEL_AR", "AUTO_AV", "AUTO_AR", "ARRET_OBSTACLE", "ARRET_FEU", "ARRET_THERMIQUE", "URGENCE", "DEFAUT"
};
static const char *const noms_entrees[AUTO_N_ENTREES] = {
    "ESTOP", "PANNE", "THERMIQUE", "OBSTACLE", "FEU", "FIN_TRAJET", "ACQUIT", "USER_ARRET", "DEM_MAN_AV", "DEM_MAN_AR", "DEM_AUTO_AV", "DEM_AUTO_AR", "INIT_OK"
};
static const char *const noms_sorties[AUTO_N_SORTIES] = {
    "moteur", "sens", "mode_auto", "led", "cause", "son", "etat_tele"
};

void automate_sorties(auto_etat_t e, auto_sorties_t *s)
{
    if (s != 0 && (uint32_t)e < AUTO_N_ETATS) {
        *s = sorties[e];
    }
}

auto_etat_t automate_transition(auto_etat_t e, auto_entree_t x, auto_sorties_t *s)
{
    if ((uint32_t)e < AUTO_N_ETATS && (uint32_t)x < AUTO_N_ENTREES) {
        e = (auto_etat_t)table[e][x];
    }
    automate_sorties(e, s);
    return e;
}

auto_entree_t automate_choisir(uint32_t masque)
{
    for (uint32_t i = 0u; i < AUTO_N_ENTREES; i++) {
        if (masque & (1u << i)) {
            return (auto_entree_t)i;
        }
    }
    return AUTO_N_ENTREES;
}

const char *automate_nom_etat(auto_etat_t e)
{
    return ((uint32_t)e < AUTO_N_ETATS) ? noms_etats[e] : "?";
}

const char *automate_nom_entree(auto_entree_t x)
{
    return ((uint32_t)x < AUTO_N_ENTREES) ? noms_entrees[x] : "-";
}

const char *automate_nom_sortie(uint32_t i)
{
    return (i < AUTO_N_SORTIES) ? noms_sorties[i] : 0;
}

uint32_t automate_valeur_sortie(const auto_sorties_t *s, uint32_t i)
{
    switch (i) {
    case 0u: return s->moteur;
    case 1u: return s->sens;
    case 2u: return s->mode_auto;
    case 3u: return s->led;
    case 4u: return s->cause;
    case 5u: return s->son;
    case 6u: return s->etat_tele;
    default: return 0u;
    }
}

/* Seconde copie de la table, sous une autre forme : triplets (etat, entree,
 * etat suivant) et somme des sorties de l'etat suivant. */
typedef struct { uint8_t e, x, suivant, somme_sorties; } auto_ref_t;
static const auto_ref_t reference[] = {
    {0,0,9,7}, {0,1,10,13}, {0,2,0,5}, {0,3,0,5}, {0,4,0,5}, {0,5,0,5},
    {0,6,0,5}, {0,7,0,5}, {0,8,0,5}, {0,9,0,5}, {0,10,0,5}, {0,11,0,5},
    {0,12,1,0}, {1,0,9,7}, {1,1,10,13}, {1,2,1,0}, {1,3,1,0}, {1,4,1,0},
    {1,5,1,0}, {1,6,1,0}, {1,7,1,0}, {1,8,2,4}, {1,9,3,7}, {1,10,4,5},
    {1,11,5,8}, {1,12,1,0}, {2,0,9,7}, {2,1,10,13}, {2,2,8,11}, {2,3,6,9},
    {2,4,2,4}, {2,5,2,4}, {2,6,2,4}, {2,7,1,0}, {2,8,2,4}, {2,9,2,4},
    {2,10,2,4}, {2,11,2,4}, {2,12,2,4}, {3,0,9,7}, {3,1,10,13}, {3,2,8,11},
    {3,3,6,9}, {3,4,3,7}, {3,5,3,7}, {3,6,3,7}, {3,7,1,0}, {3,8,3,7},
    {3,9,3,7}, {3,10,3,7}, {3,11,3,7}, {3,12,3,7}, {4,0,9,7}, {4,1,10,13},
    {4,2,8,11}, {4,3,6,9}, {4,4,7,6}, {4,5,1,0}, {4,6,4,5}, {4,7,1,0},
    {4,8,4,5}, {4,9,4,5}, {4,10,4,5}, {4,11,4,5}, {4,12,4,5}, {5,0,9,7},
    {5,1,10,13}, {5,2,8,11}, {5,3,6,9}, {5,4,7,6}, {5,5,1,0}, {5,6,5,8},
    {5,7,1,0}, {5,8,5,8}, {5,9,5,8}, {5,10,5,8}, {5,11,5,8}, {5,12,5,8},
    {6,0,9,7}, {6,1,10,13}, {6,2,8,11}, {6,3,6,9}, {6,4,6,9}, {6,5,6,9},
    {6,6,1,0}, {6,7,6,9}, {6,8,6,9}, {6,9,6,9}, {6,10,6,9}, {6,11,6,9},
    {6,12,6,9}, {7,0,9,7}, {7,1,10,13}, {7,2,8,11}, {7,3,6,9}, {7,4,7,6},
    {7,5,7,6}, {7,6,1,0}, {7,7,7,6}, {7,8,7,6}, {7,9,7,6}, {7,10,7,6},
    {7,11,7,6}, {7,12,7,6}, {8,0,9,7}, {8,1,10,13}, {8,2,8,11}, {8,3,8,11},
    {8,4,8,11}, {8,5,8,11}, {8,6,1,0}, {8,7,8,11}, {8,8,8,11}, {8,9,8,11},
    {8,10,8,11}, {8,11,8,11}, {8,12,8,11}, {9,0,9,7}, {9,1,9,7}, {9,2,9,7},
    {9,3,9,7}, {9,4,9,7}, {9,5,9,7}, {9,6,1,0}, {9,7,9,7}, {9,8,9,7},
    {9,9,9,7}, {9,10,9,7}, {9,11,9,7}, {9,12,9,7}, {10,0,9,7}, {10,1,10,13},
    {10,2,10,13}, {10,3,10,13}, {10,4,10,13}, {10,5,10,13}, {10,6,1,0}, {10,7,10,13},
    {10,8,10,13}, {10,9,10,13}, {10,10,10,13}, {10,11,10,13}, {10,12,10,13},
};

uint32_t automate_autotest(void)
{
    uint32_t ecarts = 0u;
    for (uint32_t i = 0u; i < sizeof reference / sizeof reference[0]; i++) {
        auto_sorties_t s;
        uint32_t somme = 0u;
        auto_etat_t suivant = automate_transition((auto_etat_t)reference[i].e,
                                                  (auto_entree_t)reference[i].x, &s);
        for (uint32_t k = 0u; k < AUTO_N_SORTIES; k++) {
            somme += automate_valeur_sortie(&s, k);
        }
        if ((uint32_t)suivant != reference[i].suivant || (somme & 0xFFu) != reference[i].somme_sorties) {
            ecarts++;
        }
    }
    return ecarts;
}
