#!/bin/bash
timeout --signal SIGINT $2 ../../build/wordcount config.yaml $1
