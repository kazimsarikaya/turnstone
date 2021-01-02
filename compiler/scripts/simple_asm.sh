#!/bin/sh


parse_line() {
  token_cnt=$#
  if [[ $token_cnt -eq 1 ]]; then
    echo 1 op code
  elif [[ $token_cnt -eq 2 ]]; then
    echo 2 op code
  elif [[ $token_cnt -eq 3 ]]; then
    echo 3 op code
  else
    echo empty/unknown op code
    return
  fi
}


while read line; do
  parse_line $line
done < $1
