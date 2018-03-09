#!/usr/bin/env bash

for i in `find -L $1`
do
	if test -L "$i" -a ! -e "$i" -a $(($(date '+%s'))) -ge $(($(stat -c '%Y' "$i") + 604800))
	then
		echo "$i"
	fi
done