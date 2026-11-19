/* ---------------------------------------------------------------------------
 * evq.c — file circulaire d'événements (CEG 3536, laboratoire 3) — CORRIGÉ
 * Tableau fixe de EVQ_CAPACITE éléments, indices tête (lecture) et queue
 * (écriture), compteur d'éléments. Débordement : push refusé et compté, la
 * file n'est jamais corrompue (E7).
 * ------------------------------------------------------------------------- */
#include "evq.h"
#include "projet.h"

#define EVQ_RESERVE 8u
#define EVQ_TAILLE  (EVQ_CAPACITE + EVQ_RESERVE)

static evq_event_t file[EVQ_TAILLE];
static uint32_t    tete;          /* prochain élément à lire                   */
static uint32_t    queue;         /* prochaine case à écrire                   */
static uint32_t    nombre;        /* éléments présents                         */
static uint32_t    debordements;  /* push refusés (file pleine)                */
static uint32_t    total;         /* push acceptés depuis evq_init             */
static uint32_t    occupation_max;

static bool prioritaire(evq_type_t type)
{
    return type == EV_ESTOP || type == EV_DEFAUT;
}

void evq_init(void)
{
    uint32_t p = crit_entrer();
    uint32_t t0 = CRIT_CYCLES();

    tete = queue = nombre = debordements = total = occupation_max = 0u;
    CRIT_MESURER(t0);
    crit_sortir(p);
}

bool evq_push(evq_type_t type, uint32_t valeur, uint32_t t_ms)
{
    bool     accepte = false;
    uint32_t p = crit_entrer();
    uint32_t t0 = CRIT_CYCLES();
    uint32_t limite = prioritaire(type) ? EVQ_TAILLE : EVQ_CAPACITE;

    if (nombre >= limite) {
        debordements++;
    } else {
        file[queue].type   = type;
        file[queue].valeur = valeur;
        file[queue].t_ms   = t_ms;
        queue = (queue + 1u) % EVQ_TAILLE;   /* enroulement de l'index           */
        nombre++;
        total++;
        if (nombre > occupation_max) {
            occupation_max = nombre;
        }
        accepte = true;
    }
    CRIT_MESURER(t0);
    crit_sortir(p);
    return accepte;
}

bool evq_pop(evq_event_t *e)
{
    bool     obtenu = false;
    uint32_t p;
    uint32_t t0;

    if (e == 0) {
        return false;
    }
    p = crit_entrer();
    t0 = CRIT_CYCLES();
    if (nombre != 0u) {
        *e = file[tete];
        tete = (tete + 1u) % EVQ_TAILLE;
        nombre--;
        obtenu = true;
    }
    CRIT_MESURER(t0);
    crit_sortir(p);
    return obtenu;
}

static uint32_t lire(const uint32_t *variable)
{
    uint32_t p = crit_entrer();
    uint32_t t0 = CRIT_CYCLES();
    uint32_t v = *variable;

    CRIT_MESURER(t0);
    crit_sortir(p);
    return v;
}

uint32_t evq_count(void)        { return lire(&nombre); }
uint32_t evq_debordements(void) { return lire(&debordements); }
uint32_t evq_max(void)          { return lire(&occupation_max); }
uint32_t evq_total(void)        { return lire(&total); }
