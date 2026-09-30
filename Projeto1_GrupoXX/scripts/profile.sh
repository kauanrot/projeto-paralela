#!/bin/bash
# profile.sh - coleta metricas com perf e valgrind (cachegrind)
# Uso (dentro de Projeto1_GrupoXX): bash scripts/profile.sh
LOG=../access_log_large.txt
THREADS=4
PERF=$(ls /usr/lib/linux-tools/*/perf 2>/dev/null | head -1)
mkdir -p resultados logs

# Contadores de hardware (cache-misses, cycles...) nao existem em maquinas virtuais
# como o Codespaces. Nesse caso usamos so os eventos de software.
EVENTOS="cycles,instructions,cache-references,cache-misses,branch-misses"
if $PERF stat -e cycles true 2>&1 | grep -q "not supported"; then
    echo "AVISO: sem contadores de hardware, usando so eventos de software"
    EVENTOS="task-clock,context-switches,cpu-migrations,page-faults"
fi

$PERF stat -e $EVENTOS ./bin/log_analyzer_seq $LOG > /dev/null 2> resultados/perf_seq.txt
$PERF stat -e $EVENTOS ./bin/log_analyzer_par $LOG $THREADS > /dev/null 2> resultados/perf_par.txt
$PERF stat -e $EVENTOS ./bin/log_analyzer_par_optimized $LOG $THREADS > /dev/null 2> resultados/perf_opt.txt

# cachegrind e muito lento, entao usamos so as 100 mil primeiras linhas
head -n 100000 $LOG > logs/amostra.txt
valgrind --tool=cachegrind --cache-sim=yes --cachegrind-out-file=resultados/cachegrind_seq.out \
    ./bin/log_analyzer_seq logs/amostra.txt > /dev/null 2> resultados/cachegrind_seq.txt
valgrind --tool=cachegrind --cache-sim=yes --cachegrind-out-file=resultados/cachegrind_opt.out \
    ./bin/log_analyzer_par_optimized logs/amostra.txt $THREADS > /dev/null 2> resultados/cachegrind_opt.txt

echo "Pronto! Resultados em resultados/"
