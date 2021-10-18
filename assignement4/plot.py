import matplotlib.pyplot as plt
import numpy as np

rounds_labels = np.array([1, 3, 5, 8, 15, 40])
rounds_ticks = range(len(rounds_labels))
exec_times = [138, 138, 138, 138, 138, 139]
accuracies = [640, 640, 638, 610, 640, 640]
#fname = "meltdown_segv"
#fname = "meltdown_tsx"
fname = "spectre"

a1 = plt.subplot(2, 1, 1)
a1.bar(rounds_ticks, np.array(accuracies), width=0.2,
       color='g', align='center', zorder=2)
plt.ylabel('Accuracy (nb matches over 20 tries)\n32B secret    [X]/640')
plt.grid(color='grey', zorder=0)
plt.bar_label(a1.containers[0])
plt.ylim([0, 750])
plt.xticks(rounds_ticks, rounds_labels)

plt.title(fname + " performance")
ax = plt.subplot(2, 1, 2)
plt.xticks(rounds_ticks, rounds_labels)
ax.bar(rounds_ticks, np.array([x / 1000 for x in exec_times]),
       width=0.2, color='b', align='center', zorder=2)
plt.ylabel('Execution time [s]')
plt.xlabel('Branch training iterations, 2 rounds')
plt.bar_label(ax.containers[0])
plt.ylim([0, 0.2])
plt.grid(color='grey', zorder=0)


plt.savefig(fname + '.png')
