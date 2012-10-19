#!/bin/bash
#
# Display EGADS manual
#
#
# EGADS manual text file
if [ ! -n "$EGADSROOT" ]; then  # variable $EGADSROOT is not set or is empty
  MANFILE="egman.txt"
else
  MANFILE="$EGADSROOT/egman.txt"
fi 


if [ $# == 0 ]; then 	# if no argument, display the whole egman.txt file

	more $MANFILE

else  					# otherwise display section of interst

  # number of lines in the file
  #LTOT=`wc -l $MANFILE | cut -f1 -d" "` # works on linux but not on mac
  LTOT=`grep -i -m 1 -n "%%%" $MANFILE | cut -f1 -d:`
  #echo LTOT = $LTOT

  # line number with specified function
  LNUM=`grep -i -m 1 -n "\-\-$1" $MANFILE | cut -f1 -d:`
  #echo LNUM = $LNUM

  # lines from $1 location to end of file
  let LTOEND=$LTOT-$LNUM
  #echo LTOEND = $LTOEND

  # lines in section of interest
  #LSEC=`tail -$LTOEND $MANFILE | grep -m 1 -n "\-\-[a-z,A-Z]" | cut -f1 -d:`
  LSEC=`tail -$LTOEND $MANFILE | grep -m 1 -n "\-\-\-\-" | cut -f1 -d:`
  #echo LSEC = $LSEC

  # display section of interest
  let L1=$LTOEND+1
  let L2=$LSEC+0
  echo " "
  tail -$L1 $MANFILE | head -$L2

fi
