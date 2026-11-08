/* ---------------------------------------------------------------------------
 * evq.c — file circulaire d'événements (CEG 3536, laboratoire 3, E7)
 *         — SQUELETTE À COMPLÉTER (jalon J2)
 * Tableau fixe de EVQ_CAPACITE éléments, indice de tête (lecture), indice de
 * queue (écriture), compteur d'éléments. Débordement : push refusé et compté,
 * la file n'est jamais corrompue. Aucun accès matériel : ce fichier est
 * compilé tel quel sur hôte (hote/Makefile) et sur la cible.
 * Vérification : hote/test_evq.c (9 cas) et commande série q sur la cible.
 * ------------------------------------------------------------------------- */
#include "evq.h"

static evq_event_t file[EVQ_CAPACITE];
static uint32_t    tete;          /* prochain élément à lire                   */
static uint32_t    queue;         /* prochaine case à écrire                   */
static uint32_t    nombre;        /* éléments présents                         */
static uint32_t    debordements;  /* push refusés (file pleine)                */
static uint32_t    total;         /* push acceptés depuis evq_init             */

void evq_init(void)
{
    tete = queue = nombre = debordements = total = 0u;
    file[0].type = EV_AUCUN;           /* à retirer une fois push/pop écrites */
}

/* À COMPLÉTER : si la file est pleine, debordements++ et retour false ;
 * sinon écrire type, valeur, t_ms en file[queue], avancer queue modulo
 * EVQ_CAPACITE (enroulement), nombre++, total++, retour true.              */
bool evq_push(evq_type_t type, uint32_t valeur, uint32_t t_ms)
{
    if (nombre >= EVQ_CAPACITE) {
        debordements++;                    /* squelette : refuse tout             */
        return false;
    }
    file[queue].type   = type;
    file[queue].valeur = valeur;
    file[queue].t_ms   = t_ms;
    queue = (queue + 1u) % EVQ_CAPACITE;
    nombre++;
    total++;
    return true;
}

/* À COMPLÉTER : si nombre == 0 ou e == 0, retour false ; sinon copier
 * file[tete] dans *e, avancer tete modulo EVQ_CAPACITE, nombre--, true.     */
bool evq_pop(evq_event_t *e)
{
    if (nombre == 0u || e == 0) {
        return false;
    }
    *e = file[tete];
    tete = (tete + 1u) % EVQ_CAPACITE;
    nombre--;
    return true;
}

uint32_t evq_count(void)        { return nombre; }
uint32_t evq_debordements(void) { return debordements; }
uint32_t evq_total(void)        { return total; }
