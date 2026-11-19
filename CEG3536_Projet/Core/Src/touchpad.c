/* ---------------------------------------------------------------------------
 * touchpad.c — contrôleur tactile (CEG 3536, laboratoire 4, E4)
 *              — SQUELETTE À COMPLÉTER
 *
 * Repères (énoncé, 4.7 ; registres du contrôleur MPR121, À CONFIRMER dans la
 * fiche technique liée en annexe D du guide) :
 *   1. réinitialisation logicielle : 0x80 <- 0x63 ;
 *   2. mode arrêt : 0x5E <- 0x00 (obligatoire avant toute configuration) ;
 *   3. filtres de la ligne de base : registres 0x2B à 0x40 (valeurs de la note
 *      d'application) ;
 *   4. seuils de l'électrode i : toucher 0x41 + 2i, relâchement 0x42 + 2i
 *      (valeurs de départ 12 et 6) ;
 *   5. mise en service des 12 électrodes : 0x5E <- 0x8C, EN DERNIER.
 * Chaque écriture de registre est une transaction de deux octets de données
 * {registre, valeur}. tp_init est appelée avant la boucle principale : elle
 * peut enchaîner ses transactions (une quarantaine, environ 15 ms).
 * État des touches : registres 0x00 (électrodes 0 à 7) et 0x01 (électrodes 8
 * à 11), lus en UNE transaction (i2c_ecrire_lire) ; ne garder que 12 bits.
 * Vérification au débogueur : relire 0x5E et deux seuils après tp_init.
 *
 * Si le gestionnaire EXTI6 est écrit en C, il se place ici :
 *     void EXTI6_IRQHandler(void) { thi_flag = 1u; <effacer RPR1 et FPR1> }
 * (il n'est alors pas défini dans exti_l4.s).
 * ------------------------------------------------------------------------- */
#include "lab4.h"
#include "i2c.h"
#include "touchpad.h"

#define TP_REG_ETAT             0x00u
#define TP_REG_RESET            0x80u
#define TP_VAL_RESET            0x63u
#define TP_REG_ECR              0x5Eu
#define TP_ECR_ARRET            0x00u
#define TP_ECR_12_ELECTRODES    0x8Cu
#define TP_REG_SEUIL_TOUCHER    0x41u
#define TP_REG_SEUIL_RELACHER   0x42u
#define TP_SEUIL_TOUCHER        12u
#define TP_SEUIL_RELACHER       6u
#define TP_MASQUE_ETAT          0x0FFFu

typedef struct {
    uint8_t reg;
    uint8_t val;
} tp_reg_t;

static const tp_reg_t tp_config[] = {
    { 0x2Bu, 0x01u }, { 0x2Cu, 0x01u }, { 0x2Du, 0x0Eu }, { 0x2Eu, 0x00u },
    { 0x2Fu, 0x01u }, { 0x30u, 0x05u }, { 0x31u, 0x01u }, { 0x32u, 0x00u },
    { 0x33u, 0x00u }, { 0x34u, 0x00u }, { 0x35u, 0x00u },
    { 0x5Bu, 0x00u }, { 0x5Cu, 0x10u }, { 0x5Du, 0x20u }
};

static bool ecrire_reg(uint8_t reg, uint8_t val)
{
    uint8_t d[2] = { reg, val };

    return i2c_ecrire(I2C_ADR_TOUCHPAD, d, 2u) == I2C_OK;
}

/* Table électrode -> touche : À RELEVER (toucher chaque touche et noter le bit
 * qui monte dans l'état lu ; consigner la table dans JOURNAL.md).
 * Valeurs : 0 à 9, TOUCHE_FN, TOUCHE_NEXT ; -1 = électrode sans touche connue. */
const int8_t tp_table[TP_N_ELECTRODES] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

bool tp_init(void)
{
    /* À COMPLÉTER : étapes 1 à 5 ; retourner false dès qu'une écriture échoue */
    if (!ecrire_reg(TP_REG_RESET, TP_VAL_RESET)) {
        return false;
    }
    if (!ecrire_reg(TP_REG_ECR, TP_ECR_ARRET)) {
        return false;
    }
    for (uint32_t i = 0u; i < sizeof tp_config / sizeof tp_config[0]; i++) {
        if (!ecrire_reg(tp_config[i].reg, tp_config[i].val)) {
            return false;
        }
    }
    for (uint32_t e = 0u; e < TP_N_ELECTRODES; e++) {
        if (!ecrire_reg((uint8_t)(TP_REG_SEUIL_TOUCHER + 2u * e), TP_SEUIL_TOUCHER)) {
            return false;
        }
        if (!ecrire_reg((uint8_t)(TP_REG_SEUIL_RELACHER + 2u * e), TP_SEUIL_RELACHER)) {
            return false;
        }
    }
    return ecrire_reg(TP_REG_ECR, TP_ECR_12_ELECTRODES);
}

bool tp_lire(uint16_t *etat12)
{
    /* À COMPLÉTER */
    uint8_t reg = TP_REG_ETAT;
    uint8_t d[2];

    if (i2c_ecrire_lire(I2C_ADR_TOUCHPAD, &reg, 1u, d, 2u) != I2C_OK) {
        return false;
    }
    *etat12 = (uint16_t)((((uint32_t)d[1] << 8) | d[0]) & TP_MASQUE_ETAT);
    return true;
}

/* [FOURNI] un seul nouvel appui, et aucune autre électrode touchée */
int32_t tp_touche(uint16_t avant, uint16_t apres)
{
    uint32_t a = (uint32_t)apres & 0x0FFFu;
    uint32_t nouveaux = a & ~(uint32_t)avant;

    if (nouveaux == 0u || (a & (a - 1u)) != 0u) {
        return -1;                      /* rien de nouveau, ou plusieurs touches */
    }
    for (uint32_t i = 0u; i < TP_N_ELECTRODES; i++) {
        if (nouveaux == (1u << i)) {
            return tp_table[i];
        }
    }
    return -1;
}
