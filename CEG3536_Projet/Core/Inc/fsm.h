/* ---------------------------------------------------------------------------
 * fsm.h — machine à états du véhicule en C (CEG 3536, laboratoire 3, E8)
 * Consomme les événements de la file (evq) et pilote les sorties par les
 * routines assembleur (led_set, motor_set_dir, motor_set_pwm, buzzer_bip).
 * ------------------------------------------------------------------------- */
#ifndef FSM_H
#define FSM_H

#include <stdint.h>
#include "lab3.h"
#include "evq.h"
#include "projet.h"

#define DEFAUT_I2C_RETABLI  0x80u

#ifndef PARTIE_II
#define PARTIE_II           1
#endif

void fsm_init(void);                             /* ARRÊT, sorties sûres (E1)   */
void fsm_traiter(const evq_event_t *e);          /* une transition par événement */
void fsm_sorties(void);                          /* chaque tick : rampe, DEL, buzzer */
const etat_vehicule_t *fsm_etat(void);           /* état exposé (télémétrie)    */
void     fsm_init_fin(uint32_t code_defaut);
uint32_t fsm_regulation(uint32_t consigne_trajet_rpm, uint32_t *plafond_pwm_pct);
void     fsm_pwm_regule(uint32_t pwm_pct);
bool     fsm_autonome_actif(void);
void     fsm_trajet_exposer(uint32_t segment, uint32_t dist_x10);
void     fsm_essai(uint32_t actif);
void     fsm_message_file(void);

#endif /* FSM_H */
