#!/bin/bash
NAME=heatmap

rm -f $NAME.tar

res=$(git status | grep "nothing to commit, working directory clean" -c)

if [[ $res == "0" ]]; then
	echo "ERROR: unsynched changes with git repo"
else
	git archive --format=tar HEAD > $NAME.tar
	echo "Done"
fi
