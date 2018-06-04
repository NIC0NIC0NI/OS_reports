#!/bin/bash

O=build
REP=2000000

for THREAD_NUM in {2..4..2}
do
    ${O}/test_empty_section ${THREAD_NUM} ${REP}
done

for THREAD_NUM in {2..4..2}
do
    ${O}/test_small_section ${THREAD_NUM} ${REP}
done
