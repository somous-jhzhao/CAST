### script that takes a PES or a Free Energy Surface and walks on a given path

# USER INPUT
FILENAME = "freeEnergy.csv"  # name of the csv file
PATH = [[-180, -180], [-180, -175], [-175, -175], [-170, -175], [-170, -170], [-165, -170],
        [-165, -165], [-160, -165], [-160, -160], [-155, -160], [-155, -155], [-155, -150],
        [-150, -150], [-150, -145], [-150, -140], [-150, -135], [-150, -130], [-150, -125],
        [-150, -120], [-150, -115], [-150, -110], [-150, -105], [-150, -100], [-150, -95],
        [-150, -90], [-150, -85], [-150, -80], [-150, -75], [-150, -70], [-150, -65], [-150, -60],
        [-150, -55], [-150, -50], [-150, -45], [-145, -45], [-140, -45], [-135, -45], [-130, -45],
        [-125, -45], [-120, -45], [-115, -45], [-110, -45], [-105, -45], [-100, -45], [-95, -45],
        [-90, -45], [-85, -45], [-80, -45], [-75, -45], [-70, -45], [-65, -45], [-60, -45],
        [-55, -45], [-50, -45], [-45, -45], [-40, -45], [-40, -40], [-40, -35], [-40, -30],
        [-40, -25], [-40, -20], [-40, -15], [-40, -10], [-40, -5], [-40, 0], [-40, 5], [-40, 10],
        [-40, 15], [-40, 20], [-40, 25], [-40, 30], [-40, 35], [-40, 40], [-40, 45], [-40, 50],
        [-40, 55], [-40, 60], [-40, 65], [-40, 70], [-40, 75], [-40, 80], [-40, 85], [-40, 90],
        [-40, 95], [-40, 100], [-40, 105], [-40, 110], [-40, 115], [-40, 120], [-40, 125],
        [-40, 130], [-40, 135], [-40, 140], [-40, 145], [-40, 150], [-40, 155], [-40, 160],
        [-40, 165], [-40, 170], [-40, 175], [-40, 180], [-35, 180], [-30, 180], [-25, 180],
        [-20, 180], [-15, 180], [-10, 180], [-5, 180], [0, 180], [5, 180], [10, 180], [15, 180],
        [20, 180], [25, 180], [30, 180], [35, 180], [40, 180], [45, 180], [50, 180], [55, 180],
        [60, 180], [65, 180], [70, 180], [75, 180], [80, 180], [85, 180], [90, 180], [95, 180],
        [100, 180], [105, 180], [110, 180], [115, 180], [120, 180], [125, 180], [130, 180],
        [135, 180], [140, 180], [145, 180], [150, 180], [155, 180], [160, 180], [165, 180],
        [170, 180], [175, 180], [180, 180]]

################################################################################
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# read csv file
with open(FILENAME) as pesfile:
    lines = pesfile.readlines()

# save content of csv file in matrix 'pes'
pes = []
for l in lines:
    linelist = l.split(",")
    for i,c in enumerate(linelist):
        if c.find("\n") != -1:
            c = c.replace("\n","")
            linelist[i] = c
    pes.append(linelist)

# get labels
x_labels = pes[0][1:len(pes[0])-1]  # first line
for i,x in enumerate(x_labels):
    x_labels[i] = int(x)
y_labels = []                       # first column
for i, line in enumerate(pes):
    if i != 0 and i != len(pes)-1:
        y_labels.append(int(line[0]))

# walk path
energies = []
with open("path.csv","w") as path:
    path.write("step, x, y, value\n")  # write headline
    for i,p in enumerate(PATH):
        x = x_labels.index(p[0])+1
        y = y_labels.index(p[1])+1
        value = float(pes[y][x])
        energies.append(value)
        path.write("{}, {}, {}, {}\n".format(i+1, p[0], p[1], value))

# plot path
plt.plot(energies)
plt.xlabel("Step")
plt.ylabel("Energy [kcal/mol]")
plt.savefig("path.png")
plt.close()
