/* ---------------------------------------------------------------------------
 * automate_gen.h — GENERE par outils/automate.py 1.0 a partir de automate.json
 * Automate « vehicule », somme de la table : 43985C1A
 * NE PAS MODIFIER A LA MAIN : corriger la table JSON, verifier, puis regenerer.
 * Aucun acces materiel : compile sur la cible STM32 et sur hote.
 * ------------------------------------------------------------------------- */
#ifndef AUTOMATE_GEN_H
#define AUTOMATE_GEN_H

#include <stdint.h>

#define AUTO_SOMME 0x43985C1Au      /* empreinte de la table d'origine */

typedef enum {
    AUTO_ETAT_INIT = 0,
    AUTO_ETAT_PRET = 1,
    AUTO_ETAT_MANUEL_AV = 2,
    AUTO_ETAT_MANUEL_AR = 3,
    AUTO_ETAT_AUTO_AV = 4,
    AUTO_ETAT_AUTO_AR = 5,
    AUTO_ETAT_ARRET_OBSTACLE = 6,
    AUTO_ETAT_ARRET_FEU = 7,
    AUTO_ETAT_ARRET_THERMIQUE = 8,
    AUTO_ETAT_URGENCE = 9,
    AUTO_ETAT_DEFAUT = 10,
    AUTO_N_ETATS = 11
} auto_etat_t;

/* Entrees par priorite decroissante : la premiere est la plus prioritaire. */
typedef enum {
    AUTO_ENTREE_ESTOP = 0,
    AUTO_ENTREE_PANNE = 1,
    AUTO_ENTREE_THERMIQUE = 2,
    AUTO_ENTREE_OBSTACLE = 3,
    AUTO_ENTREE_FEU = 4,
    AUTO_ENTREE_FIN_TRAJET = 5,
    AUTO_ENTREE_ACQUIT = 6,
    AUTO_ENTREE_USER_ARRET = 7,
    AUTO_ENTREE_DEM_MAN_AV = 8,
    AUTO_ENTREE_DEM_MAN_AR = 9,
    AUTO_ENTREE_DEM_AUTO_AV = 10,
    AUTO_ENTREE_DEM_AUTO_AR = 11,
    AUTO_ENTREE_INIT_OK = 12,
    AUTO_N_ENTREES = 13        /* sert aussi de valeur « aucune entree » */
} auto_entree_t;

typedef struct {
    uint8_t moteur;
    uint8_t sens;
    uint8_t mode_auto;
    uint8_t led;
    uint8_t cause;
    uint8_t son;
    uint8_t etat_tele;
} auto_sorties_t;

#define AUTO_ETAT_INITIAL AUTO_ETAT_INIT

/* Etat suivant pour l'entree x; si s != 0, *s recoit les sorties de l'etat
 * suivant. Un etat ou une entree hors domaine laisse l'etat inchange. */
auto_etat_t   automate_transition(auto_etat_t e, auto_entree_t x, auto_sorties_t *s);
/* Sorties de l'etat e (automate de Moore : les sorties ne dependent que de l'etat). */
void          automate_sorties(auto_etat_t e, auto_sorties_t *s);
/* Entree la plus prioritaire d'un masque (bit i = entree numero i);
 * AUTO_N_ENTREES si le masque est vide. */
auto_entree_t automate_choisir(uint32_t masque);
const char   *automate_nom_etat(auto_etat_t e);
const char   *automate_nom_entree(auto_entree_t x);
const char   *automate_nom_sortie(uint32_t i);      /* 0 si i >= AUTO_N_SORTIES */
uint32_t      automate_valeur_sortie(const auto_sorties_t *s, uint32_t i);
#define AUTO_N_SORTIES 7u
/* Autotest : compare automate_transition et automate_sorties a une seconde
 * copie de la table, pour chaque couple (etat, entree). Retourne le nombre
 * d'ecarts (0 = succes). Commande serie g sur la cible. */
uint32_t      automate_autotest(void);

#endif /* AUTOMATE_GEN_H */
