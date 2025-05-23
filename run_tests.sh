#!/bin/bash
set -e

LOG_DIR="../../userprog_logs"
mkdir -p "$LOG_DIR"

# 테스트 목록을 배열로 정의
test_cases=("open-boundary")

pushd userprog/build > /dev/null
make

# 각 테스트 파일에 대해 반복
for test_case in "${test_cases[@]}"; do
    echo "Running test: $test_case"
    pintos --fs-disk=10 -p tests/userprog/$test_case:$test_case -- -q -f run $test_case > "../../userprog_logs/$test_case.log" 2>&1
done

popd > /dev/null