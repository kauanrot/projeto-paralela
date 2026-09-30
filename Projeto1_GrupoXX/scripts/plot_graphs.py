#!/usr/bin/env python3
# plot_graphs.py - le resultados/resultados.json e gera os 5 graficos em graficos/
import json
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

r = json.load(open("resultados/resultados.json"))
tempo_seq = r["tempo_seq"]


def pontos(nome):
    """Devolve (lista de x, lista de y) de um dicionario do json."""
    xs = sorted(int(k) for k in r[nome])
    return xs, [r[nome][str(x)] for x in xs]


threads, t_par = pontos("strong_par")
_, t_opt = pontos("strong_opt")
speedup_par = [tempo_seq / t for t in t_par]
speedup_opt = [tempo_seq / t for t in t_opt]

# Grafico 1: speedup
plt.figure()
plt.plot(threads, threads, "k--", label="Ideal (speedup = N)")
plt.plot(threads, speedup_par, "o-", label="Mutex global")
plt.plot(threads, speedup_opt, "s-", label="Reducao local")
plt.title("Grafico 1 - Speedup vs. Numero de Threads")
plt.xlabel("Numero de threads")
plt.ylabel("Speedup (T_seq / T_par)")
plt.legend()
plt.grid(True)
plt.savefig("graficos/grafico1_speedup.png")

# Grafico 2: eficiencia
plt.figure()
plt.axhline(1.0, color="k", linestyle="--", label="Ideal (100%)")
plt.plot(threads, [s / n for s, n in zip(speedup_par, threads)], "o-", label="Mutex global")
plt.plot(threads, [s / n for s, n in zip(speedup_opt, threads)], "s-", label="Reducao local")
plt.title("Grafico 2 - Eficiencia vs. Numero de Threads")
plt.xlabel("Numero de threads")
plt.ylabel("Eficiencia (Speedup / N)")
plt.legend()
plt.grid(True)
plt.savefig("graficos/grafico2_eficiencia.png")

# Grafico 3: mutex global x reducao local
plt.figure()
plt.plot(threads, t_par, "o-", label="Mutex global")
plt.plot(threads, t_opt, "s-", label="Reducao local")
plt.axhline(tempo_seq, color="gray", linestyle=":", label="Sequencial")
plt.title("Grafico 3 - Mutex Global vs. Reducao Local")
plt.xlabel("Numero de threads")
plt.ylabel("Tempo de execucao (s)")
plt.legend()
plt.grid(True)
plt.savefig("graficos/grafico3_comparacao.png")

# Grafico 4: granularidade
blocos, g_par = pontos("gran_par")
_, g_opt = pontos("gran_opt")
blocos_kb = [b / 1024 for b in blocos]
plt.figure()
plt.plot(blocos_kb, g_par, "o-", label="Mutex global")
plt.plot(blocos_kb, g_opt, "s-", label="Reducao local")
plt.xscale("log", base=2)
plt.title("Grafico 4 - Granularidade (4 threads)")
plt.xlabel("Tamanho do bloco (KB)")
plt.ylabel("Tempo de execucao (s)")
plt.legend()
plt.grid(True)
plt.savefig("graficos/grafico4_granularidade.png")

# Grafico 5: weak scaling
threads_w, w_par = pontos("weak_par")
_, w_opt = pontos("weak_opt")
linhas = [t * r["linhas_por_thread"] for t in threads_w]
plt.figure()
plt.plot(linhas, w_par, "o-", label="Mutex global")
plt.plot(linhas, w_opt, "s-", label="Reducao local")
plt.title("Grafico 5 - Weak Scaling (carga fixa por thread)")
plt.xlabel("Tamanho do problema (linhas)")
plt.ylabel("Tempo de execucao (s)")
plt.legend()
plt.grid(True)
plt.savefig("graficos/grafico5_weak.png")

print("Graficos salvos em graficos/")
