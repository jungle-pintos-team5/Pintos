#!/bin/bash

# LOG_DIR="./userprog_logs"
# mkdir -p "$LOG_DIR"

# 테스트 목록을 배열로 정의
test_case=("write-normal")

# 각 테스트 파일에 대해 반복
source ./activate
cd userprog/build && make && pintos --fs-disk=10 -p tests/userprog/$test_case:$test_case -- -q -f run $test_case
