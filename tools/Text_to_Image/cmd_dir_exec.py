# Author:
# Shouro Chowdhury
# Center for Research on Bangla Language Processing (CRBLP)


import os
import sys
import fileinput

#how to run:
#python cmd_dir_exec.py lines.txt SolaimanLipi 24 300

inputfile=sys.argv[1]
out=inputfile.split(".")
outdirname=out[0]
fontname=sys.argv[2]
fontsize=sys.argv[3]
inputdpi=sys.argv[4]

if os.path.isdir(outdirname):
    print "dir already exists"
else:
    os.mkdir(outdirname)

num=1
gap=" "
dir=os.path.abspath(outdirname)
exebin="pango-view"
dpi="--dpi="+inputdpi
margin="--margin=20"
font="--font="+'"'+fontname+gap+fontsize+'"'
silentmode="-q"
align="--align=center"
text="--text="
#fn=fontname.split(".")
#listfile=open(dir+"/"+"list.txt", "w")

for line in fileinput.input(sys.argv[1]):
    #pngout="--output="+dir+"/"+str(num)+"_ln_"+fontname+"_"+fontsize+"_dpi_"+inputdpi+".png"
    pngout="--output="+dir+"/"+str(num)+".png"
    #txtout=dir+"/"+str(num)+"_ln_"+fontname+"_"+fontsize+"_dpi_"+inputdpi+".txt"
    txtout=dir+"/"+str(num)+".txt"
    #list=dir+"/"+str(num)+"_ln_"+fontname+"_"+fontsize+"_dpi_"+inputdpi+".png"+" "+dir+"/"+str(num)+"_ln_"+fontname+"_"+fontsize+"_dpi_"+inputdpi+".txt"
    tfile = open(txtout, "w")
    tfile.write(line)
    tfile.close()
    #listfile.write(list+"\n")
    
#cmd=exebin+gap+silentmode+gap+dpi+gap+margin+gap+align+gap+font+gap+text+'"'+line+'"'+gap+pngout
    cmd=exebin+gap+silentmode+gap+dpi+gap+margin+gap+align+gap+font+gap+text+'"'+line+'"'+gap+pngout
    os.system(cmd)
    num=num+1

#listfile.close()
