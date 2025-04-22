#!/bin/bash

# Test script for the Preemptive Priority Scheduler
echo "Compiling program..."
gcc schedule.c linkedlist.c -o schedule
gcc quick_test.c -o quick_test

echo "Running Test Case 3..."

# Function to run a test case with description
run_test() {
    echo "========================================================="
    echo "TEST CASE: $1"
    echo "Command: $2"
    echo "---------------------------------------------------------"
    $2
    echo "Test case completed."
    echo "========================================================="
    echo
}

# Test Case 3: Multiple processes with different priorities
run_test "Multiple Priority Levels" "./schedule quick_test 3 1 0 : quick_test 4 5 0 : quick_test 2 10 0"

echo "Test completed!" 