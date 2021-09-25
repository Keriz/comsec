import matplotlib.pyplot as plt
import sys
import os
import csv

data = []

for i in range(1, 4):
    with open(sys.argv[i]) as file:
        rows = csv.reader(file)
        for row in rows:
            data.append(int(row[2]))
        plt.hist(data, bins=100)
        plt.xlabel('Number of clock cycles')
        plt.ylabel('Number of occurences')
        filename, extension = os.path.splitext(sys.argv[i])
        plt.savefig(filename + '.png')
        plt.show()
