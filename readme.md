# MPI Playground

Repository for testing MPI dynamic process spawn capabilities.

Compile the MPI program.
```bash
mpic++ dynamic_spawn.cpp dynamic_spawn.o
```

Ensure that all processes use the same network interface, otherwise the program will freeze on the `MPI_Intercomm_merge` command.
```bash
mpirun --mca btl_tcp_if_include wlp0s20f3 -np 1 dynamic_spawn.o
```