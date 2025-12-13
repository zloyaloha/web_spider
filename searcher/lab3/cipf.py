import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

df = pd.read_csv('freq.csv', delimiter=';')
df = df.sort_values(by='frequency', ascending=False).reset_index(drop=True)


actual_ranks = np.arange(1, len(df) + 1)
actual_freqs = df['frequency'].values

C = actual_freqs[0]
theoretical_freqs = C / actual_ranks

plt.figure(figsize=(10, 6))

plt.loglog(actual_ranks, actual_freqs, marker='.', linestyle='none',
           markersize=2, alpha=0.6, label='Реальные данные (Корпус)')

plt.loglog(actual_ranks, theoretical_freqs, 'r--', linewidth=2,
           label=f'Закон Ципфа ($f \\approx {C} / r$)')

plt.title('Распределение терминов (Log-Log шкала)', fontsize=14)
plt.xlabel('Ранг (log)', fontsize=12)
plt.ylabel('Частота (log)', fontsize=12)
plt.legend(fontsize=12)
plt.grid(True, which="both", ls="-", alpha=0.2)

plt.tight_layout()
plt.savefig('zipf_distribution.png')
plt.show()
