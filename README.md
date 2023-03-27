# lfbench_suite

The 'lfbench_suite' consists of 5 data structures namely stack, Ordered linked list, Deque, skip-list and btree.All of these have a S/W lock-free variant based on famous lock-free algorithms or S/W based MCAS formed using single CAS (compare-and-swap). They also have H/W based counter-part using HTM. 
The code is inside src/ directory. S/W variants are all H/W agnostic and can be compiled and run on any machine with support for single-word atomics like CAS, fetch-and-increment etc.
H/W variants need HTM based systems and have been tested on ARM-TME but code is function on intel hardware with RTM support as well. 
Makefiles are provided for compilation for ARM architecture but can be easily tweaked for intel as well.

##Reference
## Build
## Benchmarking
## Tools
## Implementation
