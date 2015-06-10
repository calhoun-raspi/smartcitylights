#! /bin/bash

cd Bulb
rm -f *.o lightbulb
make clean all
python bulbemulate.py
