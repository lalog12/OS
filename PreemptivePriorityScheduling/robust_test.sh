#!/bin/bash

# Robust Test Script for Preemptive Priority Scheduler
# This script focuses on thoroughly testing both priority scheduling and SJF preemption

echo "Compiling required programs..."
gcc schedule.c linkedlist.c -o schedule
gcc quick_test.c -o quick_test

run_test() {
    echo "========================================================="
    echo "TEST CASE: $1"
    echo "Command: $2"
    echo "---------------------------------------------------------"
    echo "Expected behavior: $3"
    echo "---------------------------------------------------------"
    $2
    echo "Test case completed."
    echo "========================================================="
    echo
    sleep 2
}

echo "TESTING SJF PREEMPTION"
echo

# =============================================================================
# Test 2: Shortest Job First Preemption
# Shorter job should preempt longer job when priorities are equal
# =============================================================================
run_test "SJF Preemption (Equal Priority)" \
         "./schedule quick_test 10 5 0 : quick_test 3 5 2" \
         "Second process with burst time 3 should preempt first process with burst time 10"

echo "TESTING PRIORITY OVERRIDE OF SJF"
echo

# =============================================================================
# Test 3: Priority Override over SJF
# Higher priority process should preempt even if it has longer burst time
# =============================================================================
run_test "Priority Override over SJF" \
         "./schedule quick_test 3 5 0 : quick_test 8 10 2" \
         "Second process with priority 10 should preempt despite having longer burst time (8 vs 3)"

echo "Test completed!" 