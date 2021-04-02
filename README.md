# BRM_Code
Master thesis NTNU

Reconstruction of the clock sequence in LFSR employing irregular clocking (Binary rate multiplier) by means of bit-parallel search

# Dependencies 
- GMPLib
- Go

# Compilation and usage
## Current GOLANG version
cd evaluation/src
go run -a main.go

## Legacy compilation and usage

make
./main <polynomial degree> <output length> <errors allowed> <initial state>

make && ./main 11 8 2 1024 

