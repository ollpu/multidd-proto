#!/bin/bash

g++ --std=c++17 -O3 multidd.cpp -o multidd
g++ --std=c++17 -O3 multidd_thread.cpp -lpthread -o multidd_thread
