#include <mpi.h>

int main(int argc, char** argv)
{
    MPI_Init(&argc, &argv);

    MPI_Comm parent;
    MPI_Comm_get_parent(&parent);
    if (parent == MPI_COMM_NULL)
    {
        MPI_Comm child;
        MPI_Comm_spawn(argv[0], MPI_ARGV_NULL, 1, MPI_INFO_NULL, 0, MPI_COMM_SELF, &child, MPI_ERRCODES_IGNORE);
        
        MPI_Comm comm;
        MPI_Intercomm_merge(child, 0, &comm);
        
        int rank;
        int commSize;
        MPI_Comm_rank(comm, &rank);
        MPI_Comm_size(comm, &commSize);
        printf("I am the coordinator rank %d process, with %d processes in the communicator.\n", rank, commSize);
    }
    else
    {
        MPI_Comm comm;
        MPI_Intercomm_merge(parent, 1, &comm);

        int rank;
        int commSize;
        MPI_Comm_rank(comm, &rank);
        MPI_Comm_size(comm, &commSize);
        printf("I am just a child rank %d process, with %d processes in the communicator.\n", rank, commSize);
    }

    MPI_Finalize();
    return 0;
}