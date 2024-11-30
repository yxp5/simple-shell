#!/usr/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[1;36m'
NC='\033[0m'
USAGE="Usage: ./test.sh <executable> -t <test-dir-or-file> [-i <ignore-list>]"

program=""
tests=""
ignorelist=""

while [[ $# -gt 0 ]]; do
    case "$1" in
    -t | --test)
        tests="$2"
        shift 2
        ;;
    -i | --ignore-list)
        ignorelist="$2"
        shift 2
        ;;
    -h | --help)
        echo "$USAGE"
        exit 0
        ;;
    *)
        if [[ $program ]]; then
            echo "Error: Unknown argument"
            echo "$USAGE"
            exit 1
        fi
        program="$1"
        shift 1
        ;;
    esac
done

if [[ -z "$program" ]]; then
    echo "Error: must specify an executable to run"
    exit 1
fi

if [[ ! -x "$program" ]]; then
    echo "Error: file '$program' does not exist or is not executable"
    exit 1
fi

if [[ -n "$ignorelist" ]] && [[ ! -f "$ignorelist" ]]; then
    echo "Error: ignore list '$ignorelist' does not exist"
    exit 1
fi

function run_test() {
    executable=$(realpath "$program")
    cur_dir=$(pwd)

    test_dir=$1
    filename=$2
    name="${filename%.*}"

    echo -n -e "${NC}Running $name... "
    if [[ -f "$ignorelist" ]] && grep -q -x "$name" "$ignorelist"; then
        echo -e "${CYAN}Skipped"
        return 255
    fi

    cd "$test_dir"
    eval "$executable < $filename" >output
    for result in "$name"_result*.txt; do
        difference=$(diff -w -y --color=always output "$result")
        ret=$?
        if [[ $ret -eq 0 ]]; then
            break
        fi
    done
    if [[ $ret -eq 0 ]]; then
        echo -e "${GREEN}Passed"
    else
        echo -e "${RED}Failed\n---Diff---${NC}"
        echo "$difference"
        echo -e "${RED}---End diff---${NC}"
    fi

    rm output

    cd $cur_dir

    return $ret
}

if [[ -d "$tests" ]]; then
    files=$(ls "$tests" | grep '.txt' | grep -v "_result*")

    total=0
    passed=0
    failed=0
    skipped=0
    for test in $files; do
        total=$((total + 1))
        run_test "$tests" "$test"
        ret=$?

        if [[ $ret -eq 255 ]]; then
            skipped=$((skipped + 1))
        elif [[ $ret -eq 0 ]]; then
            passed=$((passed + 1))
        else
            failed=$((failed + 1))
        fi
    done

    echo
    echo -e "${NC}---Summary---"
    echo -e "${GREEN}Passed:  $passed"
    echo -e "${RED}Failed:  $failed"
    echo -e "${CYAN}Skipped:  $skipped"
    echo -e "${NC}Total:   $total"
elif [[ -f "$tests" ]]; then
    run_test $(dirname "$tests") $(basename "$tests")
else
    echo "Invalid test file or directory."
    exit 1
fi