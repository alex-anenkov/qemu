#!/bin/bash

while read -r one_line; do
    read -r two_line
    result=""
    value1=$(echo $one_line | grep -E -i -o '=([A-Za-z0-9]+)')
    value2=$(echo $two_line | grep -E -i -o '=([A-Za-z0-9]+)')
    [[ $value1 == $value2 ]] && result="Ok" || result="Fail"
    echo "ONE: $one_line TWO: $two_line RESULT: $result"
    [[ $result == "Fail" ]] && break
done < $1

