#!/usr/bin/env bash

set -e

sudo apt install gradle
gradle wrapper
./gradlew fatJar
