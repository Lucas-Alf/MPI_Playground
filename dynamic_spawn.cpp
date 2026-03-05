#include <mpi.h>

#include <thread>

static const int MESSAGE_TAG = 0;
static const int SYNCRONIZATION_TAG = 1;
static const int TERMINATION_TAG = 2;

MPI_Comm spawn_process(MPI_Comm comm, int argc, char** argv)
{
    // Send a message to all ranks in the communicator to synchronize the parent and child communicators
    int rank;
    int comSize;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &comSize);
    for (int i = 0; i < comSize; i++)
    {
        if (i != rank)
        {
            MPI_Send(NULL, 0, MPI_CHAR, i, SYNCRONIZATION_TAG, comm);
        }
    }

    // Spawn a child process
    MPI_Comm child;
    MPI_Comm_spawn(argv[0], MPI_ARGV_NULL, 1, MPI_INFO_NULL, 0, comm, &child, MPI_ERRCODES_IGNORE);

    // Merge the intercommunicator of the parent and child processes
    MPI_Comm intercomm;
    MPI_Intercomm_merge(child, 0, &intercomm);
    return intercomm;
}

// Run the coordinator process
void run_coordinator(int argc, char** argv)
{
    int rank;
    int commSize;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &commSize);
    printf("Coordinator process (rank %d), started with %d processes in the communicator.\n", rank, commSize);

    // Declare a communicator for the coordinator process to communicate with the child processes
    MPI_Comm comm;

    // Spawn child processes.
    // The first child needs to use MPI_COMM_SELF as the communicator.
    // The subsequent children needs to use the merged communicator of the previous spawn.
    comm = spawn_process(MPI_COMM_SELF, argc, argv);
    comm = spawn_process(comm, argc, argv);
    comm = spawn_process(comm, argc, argv);
    comm = spawn_process(comm, argc, argv);
    comm = spawn_process(comm, argc, argv);

    // Send a message to all ranks in the communicator to terminate the child processes
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &commSize);
    for (int i = 0; i < commSize; i++)
    {
        if (i != rank)
        {
            MPI_Request request;
            MPI_Isend(NULL, 0, MPI_CHAR, i, TERMINATION_TAG, comm, &request);
            MPI_Request_free(&request);
        }
    }

    // Wait for all child processes to terminate
    MPI_Barrier(comm);
}

// Run the child process
void run_child(MPI_Comm parent)
{
    // Merge the intercommunicator of the parent and child processes
    MPI_Comm comm;
    MPI_Intercomm_merge(parent, 1, &comm);

    // Print the rank and size of the merged communicator
    int rank;
    int commSize;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &commSize);
    printf("Child process (rank %d), started with %d processes in the communicator.\n", rank, commSize);

    // Work loop
    while (true)
    {
        // Wait for a message from the coordinator process
        int flag;
        MPI_Status status;
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &flag, &status);

        // If there is no message, sleep for a while and check again
        if (!flag)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        // Receive the message from the coordinator process
        MPI_Recv(NULL, 0, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, comm, MPI_STATUS_IGNORE);
        switch (status.MPI_TAG)
        {
            case SYNCRONIZATION_TAG:
            {
                // Merge the intercommunicator of the parent and child processes
                MPI_Comm_spawn(NULL, MPI_ARGV_NULL, 0, MPI_INFO_NULL, status.MPI_SOURCE, comm, &comm, MPI_ERRCODES_IGNORE);
                MPI_Intercomm_merge(comm, 1, &comm);

                // Print the rank and size of the merged communicator
                MPI_Comm_rank(comm, &rank);
                MPI_Comm_size(comm, &commSize);
                break;
            }
            case MESSAGE_TAG:
            {
                // Handle the message from the coordinator process
                printf("Rank %d received a work message from the coordinator process.\n", rank);
                break;
            }
            case TERMINATION_TAG:
            {
                // Terminate the child process
                printf("Rank %d received a termination message from the coordinator process. Terminating...\n", rank);
                MPI_Barrier(comm);
                return;
            }
            default:
            {
                printf("Rank %d received an unknown message from the coordinator process (tag: %d).\n", rank, status.MPI_TAG);
                break;
            }
        }
    }
}

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
        run_coordinator(argc, argv);
    }
    else
    {
        run_child(parent);
    }

    MPI_Finalize();
    return 0;
}