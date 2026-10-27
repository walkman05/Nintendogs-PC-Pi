# Host functions

C versions, for the PC build only, of routines the decomp doesn't contain as C: the compiler runtime and SDK
assembly listed as "Not C" in `remaining.txt`, Thumb routines and the ITCM code. One file per function, named
after it. They are written from the original's behaviour (not matching code) and never used by the DS build.
