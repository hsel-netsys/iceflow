#!/bin/bash
timeout --signal SIGINT $2 ../../build/lines2words2 config.yaml $1
