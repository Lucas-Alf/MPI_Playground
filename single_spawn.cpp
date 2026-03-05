#include <mpi.h>

int main(int argc, char** argv)
{
    MPI_Init(&argc, &argv);

    // Get the parent communicator
    MPI_Comm parent;
    MPI_Comm_get_parent(&parent);

    // If the parent communicator is null, this process is the original parent process.
    // Otherwise, it is a child process.
    if (parent == MPI_COMM_NULL)
    {
        int rank;
        int commSize;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &commSize);
        printf("I am the original parent process (rank %d), my current communicator size is: %d.\n", rank, commSize);

        // Spawn a child process
        MPI_Comm child;
        printf("Spawning one child process...\n");
        MPI_Comm_spawn(argv[0], MPI_ARGV_NULL, 1, MPI_INFO_NULL, 0, MPI_COMM_SELF, &child, MPI_ERRCODES_IGNORE);

        // Merge the intercommunicator of the parent and child processes
        MPI_Comm comm;
        MPI_Intercomm_merge(child, 0, &comm);

        // Print the rank and size of the merged communicator
        MPI_Comm_rank(comm, &rank);
        MPI_Comm_size(comm, &commSize);
        printf("I am the coordinator (rank %d), with %d processes in the communicator.\n", rank, commSize);
    }
    else
    {
        // Merge the intercommunicator of the parent and child processes
        MPI_Comm comm;
        MPI_Intercomm_merge(parent, 1, &comm);

        // Print the rank and size of the merged communicator
        int rank;
        int commSize;
        MPI_Comm_rank(comm, &rank);
        MPI_Comm_size(comm, &commSize);
        printf("I am a child process (rank %d), with %d processes in the communicator.\n", rank, commSize);
    }

    MPI_Finalize();
    return 0;
}