#!/bin/sh

function die() {
  echo $*
  exit 1
}

if test -z "$EELDIR"; then
   echo "Must set EELDIR"
   exit 1
fi

if test -z "$EELFILES"; then
   echo "Must set EELFILES"
   exit 1
fi

for FILE in $EELFILES; do
  if cmp -s $EELDIR/$FILE $FILE; then
     echo "File $FILE is unchanged"
  else
     cp $EELDIR/$FILE $FILE || die "Could not move $EELDIR/$FILE to $FILE"
     echo "Updated $FILE"
  fi
done
