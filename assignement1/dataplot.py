import matplotlib.pyplot as plt
import sys
import os
import csv


for i in range(1, 4):
    data = []
    with open(sys.argv[i]) as file:
        rows = csv.reader(file)
        for row in rows:
            data.append(int(row[2]))
        plt.hist(data, bins=100, density=True, color="skyblue")
        plt.xlabel('Access time [CPU cycles]')
        plt.ylabel('Proportion of cases')
        filename, extension = os.path.splitext(sys.argv[i])
        plt.savefig(filename + '.png')
        # plt.show()
