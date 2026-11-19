/* ---------------------------------------------------------------------------
 * spi.c — pilote SPI1 maître au niveau des registres (CMSIS, sans HAL)
 *         (CEG 3536, laboratoire 4, E7) — SQUELETTE À COMPLÉTER
 *
 * Séquence (RM0438, section SPI ; énoncé 4.7) :
 *   1. horloges : RCC_AHB2ENR (GPIOAEN, GPIODEN), RCC_APB2ENR (SPI1EN) ;
 *   2. PA5, PA6, PA7 : MODER = 10, AFRL champs 5, 6 et 7 = 5 (AF5) ;
 *      PD14 : niveau haut (BSRR) PUIS sortie (MODER = 01), pour ne jamais
 *      sélectionner le capteur par accident ;
 *   3. CR1 = MSTR | SSM | SSI | BR = 001 (PCLK2 / 4 = 1 MHz), CPOL = CPHA = 0 ;
 *   4. CR2 = DS = 0111 (8 bits) | FRXTH (RXNE dès un octet reçu) ;
 *   5. CR1 |= SPE.
 * Échange d'un octet : attendre SR.TXE, écrire l'octet dans DR par un accès de
 * 8 bits (*(volatile uint8_t *)&SPI1->DR), attendre SR.RXNE, lire DR de la
 * même façon. PD14 passe à 0 avant le premier octet et revient à 1 après le
 * dernier, une fois SR.BSY retombé.
 * Attentes : bornées (compteur de boucle ou ticks_ms) — un SPI mal configuré
 * ne doit pas bloquer la boucle principale.
 * Vérification au débogueur : SPI1->CR1, CR2, SR ; GPIOD->ODR bit 14 ; octet
 * reçu en lisant l'identifiant du capteur (env.c).
 * ------------------------------------------------------------------------- */
#include "stm32l552xx.h"
#include "lab4.h"
#include "spi.h"

#define SPI_ATTENTE_MAX     4000u
#define SPI_CS_PIN          14u

static bool attendre_sr(uint32_t masque, uint32_t etat)
{
    for (uint32_t i = 0u; i < SPI_ATTENTE_MAX; i++) {
        if ((SPI1->SR & masque) == etat) {
            return true;
        }
    }
    return false;
}

void spi_init(void)
{
    /* À COMPLÉTER : étapes 1 à 5 */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIODEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;

    GPIOA->MODER = (GPIOA->MODER & ~(GPIO_MODER_MODE5 | GPIO_MODER_MODE6 | GPIO_MODER_MODE7))
                 | (2u << GPIO_MODER_MODE5_Pos) | (2u << GPIO_MODER_MODE6_Pos)
                 | (2u << GPIO_MODER_MODE7_Pos);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(GPIO_AFRL_AFSEL5 | GPIO_AFRL_AFSEL6 | GPIO_AFRL_AFSEL7))
                  | (5u << GPIO_AFRL_AFSEL5_Pos) | (5u << GPIO_AFRL_AFSEL6_Pos)
                  | (5u << GPIO_AFRL_AFSEL7_Pos);

    GPIOD->BSRR = (1u << SPI_CS_PIN);
    GPIOD->MODER = (GPIOD->MODER & ~GPIO_MODER_MODE14) | (1u << GPIO_MODER_MODE14_Pos);

    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_0;
    SPI1->CR2 = SPI_CR2_DS_0 | SPI_CR2_DS_1 | SPI_CR2_DS_2 | SPI_CR2_FRXTH;
    SPI1->CR1 |= SPI_CR1_SPE;
}

void spi_echange(const uint8_t *emis, uint8_t *recus, uint32_t n)
{
    /* À COMPLÉTER */
    for (uint32_t i = 0u; i < SPI_ATTENTE_MAX && (SPI1->SR & SPI_SR_RXNE) != 0u; i++) {
        (void)*(volatile uint8_t *)&SPI1->DR;
    }

    GPIOD->BSRR = (1u << (SPI_CS_PIN + 16u));
    for (uint32_t i = 0u; i < n; i++) {
        if (!attendre_sr(SPI_SR_TXE, SPI_SR_TXE)) {
            break;
        }
        *(volatile uint8_t *)&SPI1->DR = (emis != 0) ? emis[i] : 0u;
        if (!attendre_sr(SPI_SR_RXNE, SPI_SR_RXNE)) {
            break;
        }
        uint8_t octet = *(volatile uint8_t *)&SPI1->DR;
        if (recus != 0) {
            recus[i] = octet;
        }
    }
    (void)attendre_sr(SPI_SR_BSY, 0u);
    GPIOD->BSRR = (1u << SPI_CS_PIN);
}
