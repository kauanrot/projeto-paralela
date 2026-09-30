#!/usr/bin/env python3
# run_tests.py - roda todos os experimentos e salva em resultados/resultados.json
# Uso (dentro de Projeto1_GrupoXX): python3 scripts/run_tests.py
import subprocess, re, json, os, sys

LOG_BASE = "../access_log_large.txt"
REPETICOES = 3                      # cada teste roda 3 vezes e usamos a media
THREADS = [1, 2, 4, 8, 16]
THREADS_WEAK = [1, 2, 4, 8]
REPETIR_ARQUIVO_STRONG = 4          # arquivo do strong scaling = log base repetido 4 vezes

seq = "./bin/log_analyzer_seq"
par = "./bin/log_analyzer_par"
opt = "./bin/log_analyzer_par_optimized"


def rodar(comando):
    """Executa o programa e devolve (tempo, saida). O tempo vem do proprio programa."""
    saida = subprocess.run(comando, capture_output=True, text=True).stdout
    tempo = float(re.search(r"TEMPO DE EXECUCAO: ([0-9.]+)", saida).group(1))
    return tempo, saida


def media(comando):
    rodar(comando)                  # primeira execucao so para "aquecer" o cache do arquivo
    tempos = [rodar(comando)[0] for _ in range(REPETICOES)]
    return sum(tempos) / len(tempos)


def sem_tempo(saida):
    """Saida sem as linhas que mudam de uma execucao para outra."""
    return [l for l in saida.split("\n") if not l.startswith(("TEMPO", "THREADS", "BLOCO", "ARQUIVO"))]


def criar_arquivo(vezes, nome):
    caminho = "logs/" + nome
    if not os.path.exists(caminho):
        conteudo = open(LOG_BASE, "rb").read()
        with open(caminho, "wb") as f:
            for _ in range(vezes):
                f.write(conteudo)
    return caminho


os.makedirs("logs", exist_ok=True)
os.makedirs("resultados", exist_ok=True)
resultados = {}

# 1) Corretude: as versoes paralelas tem que dar o mesmo resultado da sequencial
referencia = sem_tempo(rodar([seq, LOG_BASE])[1])
for programa in (par, opt):
    for t in (1, 2, 4, 8):
        igual = sem_tempo(rodar([programa, LOG_BASE, str(t)])[1]) == referencia
        print("corretude", programa, t, "OK" if igual else "DIFERENTE!")
        if not igual:
            sys.exit(1)

# 2) Strong scaling: arquivo fixo, varia as threads
arquivo = criar_arquivo(REPETIR_ARQUIVO_STRONG, "strong.txt")
resultados["tempo_seq"] = media([seq, arquivo])
resultados["strong_par"] = {}
resultados["strong_opt"] = {}
print("sequencial:", resultados["tempo_seq"])
for t in THREADS:
    resultados["strong_par"][t] = media([par, arquivo, str(t)])
    resultados["strong_opt"][t] = media([opt, arquivo, str(t)])
    print("strong", t, resultados["strong_par"][t], resultados["strong_opt"][t])

# 3) Weak scaling: mesma carga por thread (arquivo cresce junto com as threads)
linhas_base = sum(1 for _ in open(LOG_BASE, "rb"))
resultados["linhas_por_thread"] = linhas_base
resultados["weak_par"] = {}
resultados["weak_opt"] = {}
for t in THREADS_WEAK:
    arq = criar_arquivo(t, "weak_%d.txt" % t)
    resultados["weak_par"][t] = media([par, arq, str(t)])
    resultados["weak_opt"][t] = media([opt, arq, str(t)])
    print("weak", t, resultados["weak_par"][t], resultados["weak_opt"][t])

# 4) Granularidade: bloco de 1KB ate 1MB (4 threads)
resultados["gran_par"] = {}
resultados["gran_opt"] = {}
bloco = 1024
while bloco <= 1048576:
    resultados["gran_par"][bloco] = media([par, arquivo, "4", str(bloco)])
    resultados["gran_opt"][bloco] = media([opt, arquivo, "4", str(bloco)])
    print("bloco", bloco, resultados["gran_par"][bloco], resultados["gran_opt"][bloco])
    bloco *= 2

with open("resultados/resultados.json", "w") as f:
    json.dump(resultados, f, indent=2)

# Tabela de speedup e eficiencia (para colar no relatorio)
print("\nThreads | T par | Speedup | Efic. | T opt | Speedup | Efic.")
for t in THREADS:
    tp = resultados["strong_par"][t]
    to = resultados["strong_opt"][t]
    ts = resultados["tempo_seq"]
    print("%7d | %.3f | %.2f | %.0f%% | %.3f | %.2f | %.0f%%" %
          (t, tp, ts / tp, 100 * ts / tp / t, to, ts / to, 100 * ts / to / t))
