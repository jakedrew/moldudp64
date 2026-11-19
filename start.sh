#!/bin/sh
set -e

./recv 239.1.1.1 12345 8080 &
sleep 0.5

./send 239.1.1.1 12345 0.01
