#!/bin/bash

rm car
g++ car.cpp -lwiringPi -o car
strip car
chmod +x car
