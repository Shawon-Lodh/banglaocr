# Author:
# Shouro Chowdhury
# Center for Research on Bangla Language Processing (CRBLP)


import sys

inputfile=sys.argv[1]
outputfile=sys.argv[2]
numofword=sys.argv[3]

#python cmd_dir_exec.py input_file output_file word_par_line

indes=open(inputfile, "r")
outdes=open(outputfile, "w")

wholefile=indes.read()
wordlist=wholefile.split()

line=""
num=1

for word in wordlist:
    num=num+1
    line=line+word+" "
    if num==int(numofword):
        outdes.write(line.strip()+"\n")
        line=""
        num=1
outdes.write(line.strip())
indes.close()
outdes.close()
