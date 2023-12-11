#!/bin/bash
timeout --signal SIGINT $2 ../../build/lines2words1 config.yaml $1
