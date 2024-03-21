#!/bin/bash
timeout --signal SIGINT $2 ../../build/agedetection config.yaml $1 age_deploy.prototxt age_net.caffemodel
