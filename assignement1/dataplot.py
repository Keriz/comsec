import matplotlib.pyplot as plt
import csv
import numpy as np
import sys

rows = []

with open(sys.argv[1], 'r') as csvfile:
    csv = csv.reader(csvfile, delimiter=', ', quotechar='|')
    for row in csv:
        rows = np.array(row)
    rowsn = np.array(rows)
    plt.hist([int(x) for x in np.array(rows)[:,2]], bins = 50)
    plt.show()