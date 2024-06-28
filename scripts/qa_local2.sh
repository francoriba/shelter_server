#!/bin/bash

# The CDash pipeline, generate the coverage report and run testing

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
 cd ../build && cmake -GNinja -DRUN_COVERAGE=1 ..

echo "Building project..."
ninja

LAB_PATH="/home/franco/operativos2/lab1_-francoriba"

echo "###################################################"
echo "###################################################"
echo "==============Starting CDash pipeline=============="

echo "###################################################"
echo "###################################################"
echo "==============Submitting CDash report=============="
ctest -VV -S ../Pipeline.cmake > CDashSummary.log
#! The file CDashSummary.log must be included in the pull request

if [ $? -eq 0 ]; then
    echo "========CDash report submitted succesfully========="
else
    echo "===============CDash report failed================="
    #exit 1
fi

echo "###################################################"
echo "###################################################"
echo "===============Starting SonarScanner==============="

# Now we generate each report
# Generate the test report for sonar.cxx.xunit.reportPaths=build/ctest_report.xml
echo "###################################################"
echo "###################################################"
echo "===================Running CTest==================="
ctest --test-dir ../build/tests --output-junit ctest_report.xml

if [ -f ../build/tests/ctest_report.xml ]; then
    echo "=========CTest report generated succesfully========"
else
    echo "================CTest report failed================"
    exit 1
fi

# Generate the coverage report for sonar.cxx.coverage.reportPath=build/coverage.xml
echo "###################################################"
echo "###################################################"
echo "=================Running Coverage=================="
gcovr -r .. .  --xml-pretty --output=../build/coverage.xml

if [ -f ../build/coverage.xml ]; then
    echo "=======Coverage report generated succesfully======="
else
    echo "==============Coverage report failed==============="
    exit 1
fi

# Generate the cppcheck report for sonar.cxx.cppcheck.reportPath=build/cppcheck.xml
echo "###################################################"
echo "###################################################"
echo "==================Running Cppcheck================="
cppcheck --enable=all --xml --xml-version=2 ../src ../lib 2>../build/cppcheck.xml

if [ -f ../build/cppcheck.xml ]; then
    echo "=======Cppcheck report generated succesfully======="
else
    echo "==============Cppcheck report failed==============="
    exit 1
fi

# # Generate the valgrind report for sonar.cxx.valgrind.reportPath=build/valgrind.xml
# echo "###################################################"
# echo "###################################################"
# echo "=================Running Valgrind=================="
# valgrind --leak-check=full --trace-children=yes --xml=yes --xml-file="${LAB_PATH}"/build/valgrind.xml ctest --test-dir "${LAB_PATH}"/build/tests

# if [ -f ../build/valgrind.xml ]; then
#     echo "=======Valgrind report generated succesfully======="
# else
#     echo "==============Valgrind report failed==============="
#     exit 1
# fi  

# Generate the Scanbuild report for sonar.cxx.scanbuild.reportPath=build/scanbuild.xml
echo "###################################################"
echo "###################################################"
echo "=================Running scan-build================"
scan-build --status-bugs -plist --force-analyze-debug-code -analyze-headers -enable-checker alpha.core.CastSize -enable-checker alpha.core.CastToStruct -enable-checker alpha.core.Conversion -enable-checker alpha.core.IdenticalExpr -enable-checker alpha.core.SizeofPtr -enable-checker alpha.core.TestAfterDivZero --exclude external -o ../build/analyzer_reports ninja -f ../build/build.ninja

if [ -d ../build/analyzer_reports ]; then
    echo "=======Scanbuild report generated succesfully======"
else
    echo "==============Scanbuild report failed=============="
    exit 1
fi

# Generate the Clang tidy report for sonar.cxx.clangtidy.reportPath=build/clangtidy.xml
echo "###################################################"
echo "###################################################"
echo "================Running clang-tidy================="
python3 run-clang-tidy.py -checks='*' -p 'build' -header-filter='*' src > ../build/clangtidy.txt

if [ -f ../build/clangtidy.txt ]; then
    echo "======Clang tidy report generated succesfully======"
else
    echo "==============Clang tidy report failed============="
    exit 1
fi

echo "###################################################"
echo "###################################################"
echo "====All reports have been generated succesfully===="


# Must change in every project
cd ..
sonar-scanner -Dsonar.token=sqp_ffb834a3fefa53e6ed4fe1fde9d02b0efada0eac
exit 0
