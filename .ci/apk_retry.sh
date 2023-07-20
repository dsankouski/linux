#!/bin/sh
# Sometimes apk just fails for now reason
# in CI, so retry it a few times.

TRIES=5
for i in $(seq 1 $TRIES); do
	if [ $i -gt 1 ]; then
		echo "Retrying apk $@"
	fi
	apk $@ && break
	sleep 3
done
