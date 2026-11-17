#!/bin/bash
# l3d_trace.sh — traces L3-D / T10 : désassemblage à -O0 et -O2, tailles du code
# (CEG 3536, laboratoire 3). À lancer dans Git Bash, à la racine du projet
# (le dossier qui contient build.sh) :   ./l3d_trace.sh
# Résultat : dossier trace_l3d/ (envoyer desassemblage_*.txt et tailles.txt).
set -e
CLT="${CLT:-C:/ST/STM32CubeCLT_1.21.0}"
BIN="${BIN:-$CLT/GNU-tools-for-STM32/bin}"   # ou : BIN="C:/.../tools/bin" ./l3d_trace.sh
CC="$BIN/arm-none-eabi-gcc"
OBJDUMP="$BIN/arm-none-eabi-objdump"
NM="$BIN/arm-none-eabi-nm"
CPU="-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-ICore/Inc -IDrivers/CMSIS/Include -IDrivers/CMSIS/Device/ST/STM32L5xx/Include"
FSM_FN="${FSM_FN:-fsm_sorties}"        # fonction de fsm.c à désassembler
cd "$(dirname "$0")"
OUT=trace_l3d
rm -rf "$OUT"; mkdir -p "$OUT"
: > "$OUT/tailles.txt"

for O in O0 O2; do
    CF="$CPU -std=gnu11 -$O -g3 -DDEBUG -DSTM32L552xx -Wall -Wextra $INC"
    "$CC" $CF -c Core/Src/l3a_c.c -o "$OUT/l3a_$O.o"
    "$CC" $CF -c Core/Src/fsm.c   -o "$OUT/fsm_$O.o"
    {
        echo "=== motor_set_pwm_c (-$O) : source C, adresses, code machine, instructions"
        "$OBJDUMP" -d -S --disassemble=motor_set_pwm_c "$OUT/l3a_$O.o"
        echo
        echo "=== $FSM_FN (-$O)"
        "$OBJDUMP" -d -S --disassemble="$FSM_FN" "$OUT/fsm_$O.o"
    } > "$OUT/desassemblage_$O.txt"
    {
        echo "--- -$O : taille des fonctions en octets (colonne 2, en hexadécimal)"
        "$NM" -S --size-sort "$OUT/l3a_$O.o" | grep -E " [Tt] "
        "$NM" -S --size-sort "$OUT/fsm_$O.o"  | grep -E " [Tt] "
    } >> "$OUT/tailles.txt"
done

# version assembleur de motor_set_pwm (indépendante du niveau d'optimisation)
"$CC" $CPU -x assembler-with-cpp -g3 -DDEBUG -Wall -ICore/Src -c Core/Src/motor.s -o "$OUT/motor.o"
{
    echo "=== motor_set_pwm (assembleur, motor.s)"
    "$OBJDUMP" -d -S --disassemble=motor_set_pwm "$OUT/motor.o"
} > "$OUT/desassemblage_asm.txt"
{
    echo "--- assembleur : taille de motor_set_pwm"
    "$NM" -S "$OUT/motor.o" | grep -E " motor_set_pwm$"
} >> "$OUT/tailles.txt"

echo "Terminé. Fichiers dans $OUT/ :"; ls -l "$OUT"/*.txt
cat "$OUT/tailles.txt"
