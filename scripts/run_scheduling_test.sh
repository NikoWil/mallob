#!/bin/bash

set -e

testcount=1
source $(dirname "$0")/systest_commons.sh

mkdir -p .api/jobs.0/
mkdir -p .api/jobs.0/{introduced,new,pending,done}/
cleanup

# Generate  jobs
t=5
n=0
for i in {1..80}; do
    # wallclock limit of 300s, arrival @ t
    introduce_job solve-$i instances/$(cat .api/benchmark_sat2020_selection_dec|head -$i|tail -1) 300 $t
    t=$((t+30))
    n=$((n+1))
done

RDMAV_FORK_SAFE=1 PATH=build/:$PATH mpirun -np $1 --oversubscribe build/mallob \
-t=4 -l=1 -g=0 -satsolver=l -v=4 -T=24000 -ch=1 -chaf=5 -chstms=60 -appmode=fork \
-cfhl=1 -smcl=30 -hmcl=30 -mlbdps=8 -ihlbd=0 -islbd=0 -fhlbd=0 -fslbd=0 -checksums=1 -log=test_$$
