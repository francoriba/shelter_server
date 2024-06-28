#!/bin/bash -e

echo ""

# Search for 'build' directory
if [ -d "../build" ]; then
    echo "## Found directory 'build' in $(pwd)"
    echo "## Deleting build directory..."
    rm -r ../build/*
else
    echo "## Directory 'build' not found in $(pwd)"
    echo "## Creating ../build"
    mkdir ../build
fi

echo "Compiling project with coverage flags..."
#cd ../build && cmake -GNinja ..
 #cd ../build && cmake -GNinja -DRUN_TESTS=1 .. 
 cd ../build && cmake -GNinja -DRUN_COVERAGE=1 -DCMAKE_BUILD_TYPE=Release ..

echo "Building project..."
ninja 

# echo "Executing tests..."
  ctest -T Test --test-dir tests -VV 

echo "Executing coverage analysis..."

#gcovr -r /home/franco/operativos2/lab1_-francoriba . --cobertura-pretty --cobertura ../build/coverage.xml

gcovr -r /home/franco/operativos2/lab1_-francoriba .  
