/* ---------------------------------------------------------------------------
 * telem.c — télémétrie LPUART1 au niveau des registres (CMSIS, sans HAL)
 *           (CEG 3536, laboratoire 3) — CORRIGÉ
 *
 * LPUART1 : TX = PG7, RX = PG8 (AF8), 115 200 bauds, 8N1, vers le port COM
 * virtuel du ST-LINK (guide, p. 54-55). Le port G (broches 2 à 15) est alimenté
 * par VDDIO2 : PWR_CR2.IOSV doit être mis à 1 (RM0438, section PWR).
 * Horloge noyau = PCLK1 = 4 MHz (RCC_CCIPR1.LPUART1SEL = 00) :
 *   BRR = 256 x 4 000 000 / 115 200 = 8 889 (fck >= 3 x bauds : respecté).
 * FIFO activée (8 octets) : telem_step remplit la FIFO à chaque tick sans
 * attendre ; une trame de 60 caractères toutes les 200 ms occupe ~ 2,5 % de la
 * capacité de la liaison (11,5 ko/s).
 * ------------------------------------------------------------------------- */
#include "stm32l552xx.h"
#include "telem.h"
#include "projet.h"

#define TX_TAILLE 1024u
static char     tx_file[TX_TAILLE];
static uint32_t tx_tete, tx_queue, tx_nombre, pertes;

void telem_init(uint32_t bauds)
{
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    (void)RCC->APB1ENR1;
    PWR->CR2 |= PWR_CR2_IOSV;                         /* alimentation du port G */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOGEN;
    RCC->APB1ENR2 |= RCC_APB1ENR2_LPUART1EN;
    RCC->CCIPR1 &= ~RCC_CCIPR1_LPUART1SEL;            /* 00 = PCLK1             */
    (void)RCC->APB1ENR2;

    /* PG7 (TX) : AFRL champ 7 = 8 ; PG8 (RX) : AFRH champ 0 = 8 ; MODER = 10 */
    GPIOG->AFR[0] = (GPIOG->AFR[0] & ~GPIO_AFRL_AFSEL7) | (8u << GPIO_AFRL_AFSEL7_Pos);
    GPIOG->AFR[1] = (GPIOG->AFR[1] & ~GPIO_AFRH_AFSEL8) | (8u << GPIO_AFRH_AFSEL8_Pos);
    GPIOG->MODER = (GPIOG->MODER & ~(GPIO_MODER_MODE7 | GPIO_MODER_MODE8))
                 | GPIO_MODER_MODE7_1 | GPIO_MODER_MODE8_1;
    GPIOG->OSPEEDR |= GPIO_OSPEEDR_OSPEED7_0;         /* vitesse moyenne        */
    GPIOG->PUPDR = (GPIOG->PUPDR & ~GPIO_PUPDR_PUPD8) | GPIO_PUPDR_PUPD8_0; /* RX tiré à 1 */

    LPUART1->CR1 = 0u;
    LPUART1->BRR = (256u * HORLOGE_HZ_C + bauds / 2u) / bauds;   /* 8 889 */
    LPUART1->CR2 = 0u;
    LPUART1->CR3 = 0u;
    LPUART1->CR1 = USART_CR1_FIFOEN | USART_CR1_TE | USART_CR1_RE;
    LPUART1->CR1 |= USART_CR1_UE;

    tx_tete = tx_queue = tx_nombre = pertes = 0u;
}

bool telem_put(const char *s)
{
    uint32_t n = 0u;

    while (s[n] != '\0') {
        n++;
    }
    if (tx_nombre + n > TX_TAILLE) {
        pertes += n;
        return false;
    }
    while (*s) {
        tx_file[tx_queue] = *s++;
        tx_queue = (tx_queue + 1u) % TX_TAILLE;
        tx_nombre++;
    }
    return true;
}

void telem_step(void)
{
    /* remplit la FIFO matérielle (8 octets) tant qu'elle n'est pas pleine */
    while (tx_nombre > 0u && (LPUART1->ISR & USART_ISR_TXE_TXFNF)) {
        LPUART1->TDR = (uint8_t)tx_file[tx_tete];
        tx_tete = (tx_tete + 1u) % TX_TAILLE;
        tx_nombre--;
    }
}

int telem_rx(void)
{
    if (LPUART1->ISR & USART_ISR_RXNE_RXFNE) {
        return (int)(LPUART1->RDR & 0xFFu);
    }
    if (LPUART1->ISR & USART_ISR_ORE) {               /* débordement : effacer */
        LPUART1->ICR = USART_ICR_ORECF;
    }
    return -1;
}

uint32_t telem_pertes(void) { return pertes; }

/* ---- formatage entier ------------------------------------------------- */
char *telem_fmt_u32(char *p, uint32_t v)
{
    char tmp[11];
    int n = 0;
    do { tmp[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (n) *p++ = tmp[--n];
    *p = '\0';
    return p;
}

char *telem_fmt_i32(char *p, int32_t v)
{
    if (v < 0) { *p++ = '-'; v = -v; }
    return telem_fmt_u32(p, (uint32_t)v);
}

/* ---- trames ------------------------------------------------------------ */
static char *fmt_hex2(char *p, uint32_t v)
{
    static const char chiffres[] = "0123456789ABCDEF";

    *p++ = chiffres[(v >> 4) & 0xFu];
    *p++ = chiffres[v & 0xFu];
    return p;
}

static void terminer_ligne(char *debut, char *p)
{
    uint32_t somme = trame_xor8((const uint8_t *)debut, (uint32_t)(p - debut));

    *p++ = '*';
    p = fmt_hex2(p, somme);
    *p++ = '\r'; *p++ = '\n'; *p = '\0';
    telem_put(debut);
}

void telem_entete(void)
{
    static const char texte[] = "H,ticks_ms,etat,sens,consigne_pct,pwm_pct,temp_c_x10,vitesse_rpm_x100,"
                                "nb_evenements,evq_debordements,mode,cause,plafond_pct,env_t_x10,"
                                "env_h_x10,env_p_x10,i2c_erreurs,ticks_perdus,mode_essai,segment,dist_x10,"
                                "consigne_rpm,code_defaut,evq_max,crit_max_cycles";
    char ligne[320];
    char *p = ligne;

    for (const char *s = texte; *s != '\0'; s++) {
        *p++ = *s;
    }
    terminer_ligne(ligne, p);
}

void telem_balayage(const uint8_t *adresses, uint32_t n)
{
    char ligne[96];
    char *p = ligne;

    *p++ = 'I'; *p++ = ',';
    p = telem_fmt_u32(p, n);
    for (uint32_t i = 0u; i < n; i++) {
        *p++ = ',';
        p = fmt_hex2(p, adresses[i]);
    }
    terminer_ligne(ligne, p);
}

void telem_evenement(const evq_event_t *e)
{
    char ligne[64];
    char *p = ligne;

    *p++ = 'E'; *p++ = ',';
    p = telem_fmt_u32(p, e->t_ms);           *p++ = ',';
    p = telem_fmt_u32(p, (uint32_t)e->type); *p++ = ',';
    p = telem_fmt_u32(p, e->valeur);
    terminer_ligne(ligne, p);
}

void telem_trame(const etat_vehicule_t *v, int32_t temp_c_x10,
                 uint32_t nb_evenements, uint32_t debordements,
                 const env_mesure_t *env, uint32_t i2c_erreurs,
                 uint32_t ticks_perdus)
{
    char ligne[224];
    char *p = ligne;
    uint32_t rpm_x100 = (uint32_t)(((uint64_t)v->vitesse_rpm_q16 * 100u + 32768u) >> 16);

    *p++ = 'T'; *p++ = ',';
    p = telem_fmt_u32(p, ticks_ms);          *p++ = ',';
    p = telem_fmt_u32(p, v->etat);           *p++ = ',';
    p = telem_fmt_u32(p, v->sens);           *p++ = ',';
    p = telem_fmt_u32(p, v->consigne_pct);   *p++ = ',';
    p = telem_fmt_u32(p, v->pwm_pct);        *p++ = ',';
    p = telem_fmt_i32(p, temp_c_x10);        *p++ = ',';
    p = telem_fmt_u32(p, rpm_x100);          *p++ = ',';
    p = telem_fmt_u32(p, nb_evenements);     *p++ = ',';
    p = telem_fmt_u32(p, debordements);      *p++ = ',';
    p = telem_fmt_u32(p, v->mode);           *p++ = ',';
    p = telem_fmt_u32(p, v->cause);          *p++ = ',';
    p = telem_fmt_u32(p, v->plafond_pct);    *p++ = ',';
    p = telem_fmt_i32(p, env->valide ? env->t_c10 : 0);           *p++ = ',';
    p = telem_fmt_u32(p, env->valide ? env->h_x10 : 0u);          *p++ = ',';
    p = telem_fmt_u32(p, env->valide ? env->p_hpa_x10 : 0u);      *p++ = ',';
    p = telem_fmt_u32(p, i2c_erreurs);       *p++ = ',';
    p = telem_fmt_u32(p, ticks_perdus);      *p++ = ',';
    p = telem_fmt_u32(p, v->mode_essai);     *p++ = ',';
    p = telem_fmt_u32(p, v->segment);        *p++ = ',';
    p = telem_fmt_u32(p, v->dist_x10);       *p++ = ',';
    p = telem_fmt_u32(p, v->consigne_rpm);   *p++ = ',';
    p = telem_fmt_u32(p, v->code_defaut);    *p++ = ',';
    p = telem_fmt_u32(p, evq_max());         *p++ = ',';
    p = telem_fmt_u32(p, crit_max_cycles);
    terminer_ligne(ligne, p);
}
