#!/bin/bash

RAN_DIR=~/ios-mcn-ran-0.4.0.iosmcn.ran
LOG_DIR=/tmp/ue_logs
mkdir -p $LOG_DIR

sudo pkill -f nr-uesoftmodem 2>/dev/null
sleep 2

start_ue() {
  local ue_num=$1
  local conf=$2
  echo "[$(date +%T)] Starting UE${ue_num}..."
  sudo nr-uesoftmodem \
    -O $RAN_DIR/conf/${conf} \
    --rfsim \
    --rfsimulator.serveraddr 127.0.0.1 \
    -C 3619200000 -r 106 --numerology 1 --ssb 516 \
    --num-ues 1 -E \
    > $LOG_DIR/ue${ue_num}.log 2>&1 &
  echo "[$(date +%T)] UE${ue_num} PID=$!"
  local iface="oaitun_ue${ue_num}"
  echo "[$(date +%T)] Waiting for ${iface} (up to 40s)..."
  for i in $(seq 1 40); do
    ip link show ${iface} &>/dev/null && break
    sleep 1
  done
  ip link show ${iface} &>/dev/null \
    && echo "[$(date +%T)] UE${ue_num} UP on ${iface}" \
    || echo "[$(date +%T)] WARNING: ${iface} not found - check $LOG_DIR/ue${ue_num}.log"
  sleep 3
}

start_ue 1 "ue1.sa.conf"
start_ue 2 "ue2.sa.conf"
start_ue 3 "ue3.sa.conf"

echo "[$(date +%T)] All UEs launched. Logs: $LOG_DIR/ue{1,2,3}.log"
echo "Press Ctrl+C to kill all"
trap "sudo pkill -f nr-uesoftmodem 2>/dev/null; exit 0" INT
wait
