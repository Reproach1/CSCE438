import matplotlib.pyplot as plt

times = []
counts = []

f = open("times_part2", "r")
for line in f:
    time = line[0:2]
    if (time[0] == '0'):
        time = time[1]
    count = line[line.find(' ') + 1:len(line)-1]

    times.append(int(time))
    counts.append(int(count))

print(counts)

plt.bar(times, counts)
plt.title("Time of Day When Usually People Go To Sleep")
plt.xlabel("Time (Hour)")
plt.ylabel("Number of Tweets")
plt.show()