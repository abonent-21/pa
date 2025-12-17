import matplotlib.pyplot as plt

# Ваши данные
procs = [1, 4, 25]
ideal = [1, 4, 25]

# Ускорение (данные из таблицы)
s_2000 = [1.0, 4.00, 2.92]  # Большая задача
s_500  = [1.0, 3.96, 2.05]  # Средняя
s_100  = [1.0, 4.79, 0.49]  # Малая

plt.figure(figsize=(10, 6))

# Рисуем линии
plt.plot(procs, ideal, 'k--', label='Идеальное ускорение', alpha=0.5)
plt.plot(procs, s_2000, 'go-', linewidth=3, label='N=2000 (Большая матрица)')
plt.plot(procs, s_500,  'bo-', label='N=500 (Средняя матрица)')
plt.plot(procs, s_100,  'rs-', label='N=100 (Малая матрица)')

# Настройки
plt.title('График ускорения (MPI Cannon)', fontsize=14)
plt.xlabel('Количество процессов', fontsize=12)
plt.ylabel('Ускорение', fontsize=12)
plt.grid(True)
plt.legend()
plt.xticks([1, 4, 25])

# Сохранение
plt.savefig('graph.png')
print("График построен!")
