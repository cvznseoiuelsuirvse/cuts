#!/bin/bash

# ninja -C build && scp -P 22000 ./build/main user@localhost:~
rsync -av -e "ssh -p 22000" --exclude='.git' --filter=':- .gitignore' ../$(basename $(pwd)) user@localhost:~
