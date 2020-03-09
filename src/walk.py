import sys
import os

nfiles=0
ndirs=0

for root, dirs, files in os.walk(sys.argv[1]):
    ndirs += len(dirs)
    nfiles += len(files)

print(f'ndirs: {ndirs}\nnfiles: {nfiles}')


