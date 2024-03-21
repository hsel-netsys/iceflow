#!/bin/bash
timeout --signal SIGINT $2 ../../build/text2lines config.yaml sourcetext.txt $1
