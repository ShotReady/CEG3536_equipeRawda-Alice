/* ---------------------------------------------------------------------------
 * tableau.c — formatage du tableau de bord (CEG 3536, laboratoire 4, E3, E8)
 *             — SQUELETTE À COMPLÉTER
 * AUCUN accès matériel, aucun en-tête CMSIS, pas de printf : ce fichier est
 * compilé tel quel sur la cible et sur hôte (hote/test_tableau.c, E11).
 * Les formats sont définis dans tableau.h ; l'autotest (tableau_test.c) en
 * vérifie six cas — les faire passer sur hôte AVANT de brancher l'afficheur.
 * ------------------------------------------------------------------------- */
#include "tableau.h"

/* [FOURNI] remplit une ligne de 16 espaces, terminée par '\0' */
static void ligne_vide(char l[17])
{
    for (uint32_t i = 0u; i < 16u; i++) {
        l[i] = ' ';
    }
    l[16] = '\0';
}

/* [FOURNI] copie s dans l à partir de la colonne col, sans dépasser 16 ni
 * écrire le '\0' de s ; retourne la colonne qui suit                          */
static uint32_t placer(char l[17], uint32_t col, const char *s)
{
    while (*s != '\0' && col < 16u) {
        l[col++] = *s++;
    }
    return col;
}

/* Écrit x10 / 10 avec une décimale, cadré à droite sur largeur caractères, à
 * partir de la colonne col ; retourne la colonne qui suit.
 * Exemples (largeur 6) : 2453 -> " 245.3" ; 0 -> "   0.0" ; -52 -> "  -5.2".
 * À COMPLÉTER (chiffres produits de droite à gauche ; signe ; espaces).       */
static uint32_t placer_fixe1(char l[17], uint32_t col, int32_t x10, uint32_t largeur)
{
    char     chiffres[12];
    uint32_t n = 0u;
    uint32_t a = (x10 < 0) ? (0u - (uint32_t)x10) : (uint32_t)x10;

    chiffres[n++] = (char)('0' + a % 10u);
    chiffres[n++] = '.';
    a /= 10u;
    do {
        chiffres[n++] = (char)('0' + a % 10u);
        a /= 10u;
    } while (a != 0u);
    if (x10 < 0) {
        chiffres[n++] = '-';
    }
    for (uint32_t i = 0u; i < largeur && col + i < 16u; i++) {
        l[col + i] = (n > largeur) ? '*' : ' ';
    }
    if (n <= largeur) {
        for (uint32_t i = 0u; i < n && col + largeur - 1u - i < 16u; i++) {
            l[col + largeur - 1u - i] = chiffres[i];
        }
    }
    return col + largeur;
}

static uint32_t trajet_nb, trajet_total;

void tableau_trajet_definir(uint32_t nb_segments, uint32_t total_tours)
{
    trajet_nb = nb_segments;
    trajet_total = total_tours;
}

static void placer_entier3(char l[17], uint32_t col, uint32_t x)
{
    if (x > 999u) {
        x = 999u;
    }
    l[col]      = (char)('0' + x / 100u);
    l[col + 1u] = (char)('0' + (x / 10u) % 10u);
    l[col + 2u] = (char)('0' + x % 10u);
}

static const char *texte_etat(const etat_vehicule_t *v)
{
    switch (v->etat) {
    case ETAT_ARRET:
        return (v->cause == CAUSE_FIN) ? "FIN" : "ARRET";
    case ETAT_MARCHE_AVANT:
    case ETAT_MARCHE_ARRIERE:
        return "MARCHE";
    case ETAT_ARRET_URGENCE:
        return "URGENCE";
    case ETAT_INIT:
        return "INIT";
    case ETAT_DEFAUT:
        return "DEFAUT";
    default:
        if (v->cause == CAUSE_THERMIQUE) {
            return "THERMIQUE";
        }
        if (v->cause == CAUSE_OBSTACLE) {
            return "OBSTACLE";
        }
        if (v->cause == CAUSE_FEU) {
            return "FEU ROUGE";
        }
        if (v->cause == CAUSE_DEFAUT) {
            return "DEFAUT";
        }
        return "ARRET";
    }
}

static void page_conduite(const etat_vehicule_t *v, char l1[17], char l2[17])
{
    uint32_t col;
    uint32_t x10 = (uint32_t)(((uint64_t)v->vitesse_rpm_q16 * 10u + 32768u) >> 16);

    placer(l1, 0u, (v->mode == MODE_AUTONOME) ? "AUT" : "MAN");
    if (v->mode_essai != 0u || (v->drapeaux & DRAPEAU_ESSAI) != 0u) {
        l1[3] = '*';
    }
    if (v->etat == ETAT_MARCHE_AVANT) {
        placer(l1, 4u, "AV");
    } else if (v->etat == ETAT_MARCHE_ARRIERE) {
        placer(l1, 4u, "AR");
    } else {
        placer(l1, 4u, "--");
    }
    placer(l1, 7u, ((v->drapeaux & DRAPEAU_FILE) != 0u) ? "FILE" : texte_etat(v));

    l2[0] = 'C';
    placer_entier3(l2, 1u, v->consigne_pct);
    l2[4] = '%';
    l2[6] = 'V';
    col = placer_fixe1(l2, 7u, (int32_t)x10, 6u);
    l2[col + 1u] = 'P';
    l2[col + 2u] = (v->plafond_pct >= 100u) ? 'X' : (char)('0' + (v->plafond_pct / 10u) % 10u);
}

static void page_trajet(const etat_vehicule_t *v, char l1[17], char l2[17])
{
    uint32_t x10 = (uint32_t)(((uint64_t)v->vitesse_rpm_q16 * 10u + 32768u) >> 16);
    uint32_t col;

    l1[0] = 'S';
    l1[1] = (char)('0' + v->segment % 10u);
    l1[2] = '/';
    l1[3] = (char)('0' + trajet_nb % 10u);
    l1[5] = 'D';
    placer_entier3(l1, 7u, v->dist_x10 / 10u);
    l1[10] = '.';
    l1[11] = (char)('0' + v->dist_x10 % 10u);
    l1[12] = '/';
    placer_entier3(l1, 13u, trajet_total);

    l2[0] = 'R';
    l2[5] = 'V';
    col = placer_fixe1(l2, 6u, (int32_t)x10, 5u);
    if (v->mode == MODE_AUTONOME && v->consigne_rpm != 0u) {
        uint32_t c10 = v->consigne_rpm * 10u;
        int32_t  ecart = (int32_t)c10 - (int32_t)x10;
        uint32_t a = (ecart < 0) ? (uint32_t)(-ecart) : (uint32_t)ecart;
        uint32_t pct = (a * 100u + c10 / 2u) / c10;

        if (pct > 99u) {
            pct = 99u;
        }
        placer_entier3(l2, 1u, v->consigne_rpm);
        l2[col + 1u] = (ecart < 0) ? '-' : '+';
        l2[col + 2u] = (char)('0' + pct / 10u);
        l2[col + 3u] = (char)('0' + pct % 10u);
    } else {
        placer(l2, 1u, "---");
        placer(l2, col + 1u, "---");
    }
    l2[15] = '%';
}

static void page_capteurs(const env_mesure_t *m, char l1[17], char l2[17])
{
    uint32_t col;

    if (m->valide != 0u) {
        l1[0] = 'T';
        col = placer_fixe1(l1, 1u, m->t_c10, 5u);
        placer(l1, col, "C  H");
        col = placer_fixe1(l1, 10u, (int32_t)m->h_x10, 5u);
        placer(l1, col, "%");
        l2[0] = 'P';
        col = placer_fixe1(l2, 1u, (int32_t)m->p_hpa_x10, 6u);
        placer(l2, col, "hPa A");
    } else {
        placer(l1, 0u, "T --.-C  H --.-%");
        placer(l2, 0u, "P ---.-hPa A");
    }
    placer_fixe1(l2, 12u, m->ta_c10, 4u);
}

void tableau_formater(const etat_vehicule_t *v, const env_mesure_t *m,
                      uint32_t page, char l1[17], char l2[17])
{
    ligne_vide(l1);
    ligne_vide(l2);

    /* À COMPLÉTER : les trois pages de tableau.h.
     * Vitesse en dixièmes de tr/min, arrondie au plus proche :
     *     (uint32_t)(((uint64_t)v->vitesse_rpm_q16 * 10u + 32768u) >> 16)     */
    if (page == PAGE_CAPTEURS) {
        page_capteurs(m, l1, l2);
    } else if (page == PAGE_TRAJET) {
        page_trajet(v, l1, l2);
    } else {
        page_conduite(v, l1, l2);
        if (page == PAGE_VERROU) {
            ligne_vide(l1);
            placer(l1, 0u, "VERROU");
        }
    }
}

uint8_t tableau_del(const etat_vehicule_t *v)
{
    /* À COMPLÉTER : image logique décrite dans tableau.h */
    uint8_t image = 0u;

    if ((v->drapeaux & DRAPEAU_LIMITATION) != 0u) {
        image |= DEL_Y1;
    }
    if (v->mode == MODE_AUTONOME) {
        image |= DEL_Y2;
    }
    if ((v->drapeaux & DRAPEAU_TOUCH) != 0u) {
        image |= DEL_Y3;
    }
    if ((v->drapeaux & DRAPEAU_FIN) != 0u) {
        image |= DEL_Y4;
    }
    if ((v->drapeaux & DRAPEAU_FEU) != 0u) {
        image |= DEL_R1;
    }
    if ((v->drapeaux & DRAPEAU_OBSTACLE) != 0u) {
        image |= DEL_R2;
    }
    if ((v->drapeaux & (DRAPEAU_DEFAUT | DRAPEAU_THERMIQUE)) != 0u || v->etat == ETAT_DEFAUT) {
        image |= DEL_R3;
    }
    if (v->etat == ETAT_ARRET_URGENCE) {
        image |= DEL_R4;
    }
    return image;
}
