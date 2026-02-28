#!/bin/bash

ninja -C build && scp -P 22000 ./build/main user@localhost:~
