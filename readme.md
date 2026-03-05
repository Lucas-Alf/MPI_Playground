# MPI Playground

Repository for testing MPI dynamic process spawn capabilities.

Ensure that all processes use the same network interface, otherwise the program will freeze on the `MPI_Intercomm_merge` command.
```bash
mpirun --mca btl_tcp_if_include wlp0s20f3 -np 1 single_spawn.out
```