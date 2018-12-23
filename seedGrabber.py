import glob, os

#Grab a bunch of seeds from separate batches and put them together.

RESULTS_DIR = "./"

winning = []
losing = []
unknown = []

def append_seeds(fileName, seedList):
	with open(fileName, 'r') as file:
		for line in file:
			seed, rest = line.split(None, 1)
			seedList.append(seed)

def output_seeds(seedList, outFileName):
	seedList.sort()
	with open(outFileName, 'w+t') as file:
		for seed in seedList:
			file.write(seed)
			file.write('\n')

for file in glob.iglob(os.path.join(RESULTS_DIR, "**/*"), recursive=True):
	if file.endswith("winning_seeds.txt"):
		print('Getting winning seeds from file: ', file)
		append_seeds(file, winning)
	elif file.endswith("losing_seeds.txt"):
		print('Getting losing seeds from file: ', file)
		append_seeds(file, losing)
	elif file.endswith("unknown_seeds.txt"):
		print('Getting unknown seeds from file: ', file)
		append_seeds(file, unknown)

output_seeds(winning, 'winning_seeds_list.txt')
output_seeds(losing, 'losing_seeds_list.txt')
output_seeds(unknown, 'unknown_seeds_list.txt')

print('Done. Program terminated successfully.')

