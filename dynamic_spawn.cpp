#include <mpi.h>

#include <thread>

static const int MESSAGE_TAG = 0;
static const int SYNCRONIZATION_TAG = 1;
static const int TERMINATION_TAG = 2;
static const int REMOVE_TAG = 3;

// Send a message to all ranks in the communicator to synchronize the parent and child communicators
void broadcast_sync_message(MPI_Comm comm)
{
    int rank;
    int comSize;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &comSize);
    for (int i = 0; i < comSize; i++)
    {
        if (i != rank)
        {
            MPI_Request request;
            MPI_Isend(0, 0, MPI_INT, i, SYNCRONIZATION_TAG, comm, &request);
            MPI_Request_free(&request);
        }
    }
}

// Spawn a new child MPI process
MPI_Comm spawn_process(MPI_Comm comm, int argc, char** argv)
{
    // Broadcast a message to all ranks to synchronize
    broadcast_sync_message(comm);

    // Spawn a child process
    MPI_Comm child;
    MPI_Comm_spawn(argv[0], MPI_ARGV_NULL, 1, MPI_INFO_NULL, 0, comm, &child, MPI_ERRCODES_IGNORE);

    // Merge the intercommunicator of the parent and child processes
    MPI_Comm intercomm;
    MPI_Intercomm_merge(child, 0, &intercomm);
    MPI_Comm_free(&child);
    return intercomm;
}

MPI_Comm remove_process(MPI_Comm comm, int rankToRemove)
{
    // Send a remove message to all ranks in the communicator to remove the specified rank
    int rank;
    int comSize;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &comSize);
    for (int i = 0; i < comSize; i++)
    {
        if (i != rank)
        {
            MPI_Request request;
            MPI_Isend(&rankToRemove, 1, MPI_INT, i, REMOVE_TAG, comm, &request);
            MPI_Request_free(&request);
        }
    }

    // Create a new communicator that excludes the specified rank
    MPI_Group group;
    MPI_Comm_group(comm, &group);
    MPI_Group newGroup;
    int ranks_to_exclude[1] = {rankToRemove};
    MPI_Group_excl(group, 1, ranks_to_exclude, &newGroup);
    MPI_Comm newComm;
    MPI_Comm_create(comm, newGroup, &newComm);
    MPI_Group_free(&group);
    MPI_Group_free(&newGroup);
    MPI_Comm_free(&comm);
    return newComm;
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
    // The subsequent children needs to use the merged communicator of the previous child.
    printf("--- Spawning child processes ---\n");
    comm = spawn_process(MPI_COMM_SELF, argc, argv);
    comm = spawn_process(comm, argc, argv);
    comm = spawn_process(comm, argc, argv);
    comm = spawn_process(comm, argc, argv);
    comm = spawn_process(comm, argc, argv);

    // Remove child processes
    // Note that after removing a child process the ranks will be reordered.
    // Exemple: if the communicator has 5 ranks (0, 1, 2, 3, 4) and we remove the rank 1, 
    // the new communicator will have 4 ranks (0, 2, 3, 4) and the new ranks will be reorderd (0, 1, 2, 3).
    std::this_thread::sleep_for(std::chrono::seconds(5));
    printf("--- Removing child processes ---\n");
    comm = remove_process(comm, 1);
    
    // Send a message to all ranks in the communicator to terminate the child processes
    std::this_thread::sleep_for(std::chrono::seconds(5));
    printf("--- Terminating program ---\n");
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &commSize);
    for (int i = 0; i < commSize; i++)
    {
        if (i != rank)
        {
            MPI_Request request;
            MPI_Isend(0, 0, MPI_INT, i, TERMINATION_TAG, comm, &request);
            MPI_Request_free(&request);
        }
    }
}

// Run the child process
void run_child(MPI_Comm parent)
{
    // Merge the intercommunicator of the parent and child processes
    MPI_Comm comm;
    MPI_Intercomm_merge(parent, 1, &comm);
    MPI_Comm_free(&parent);

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
        int message = 0;
        MPI_Recv(&message, 1, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, comm, MPI_STATUS_IGNORE);
        switch (status.MPI_TAG)
        {
            case MESSAGE_TAG:
            {
                // Handle the message from the coordinator process
                printf("Rank %d received a work message from the coordinator process.\n", rank);
                break;
            }
            case SYNCRONIZATION_TAG:
            {
                // Merge the intercommunicator of the parent and child processes
                // and free the old communicators
                MPI_Comm newComm;
                MPI_Comm mergedComm;
                MPI_Comm_spawn(NULL, MPI_ARGV_NULL, 0, MPI_INFO_NULL, status.MPI_SOURCE, comm, &newComm, MPI_ERRCODES_IGNORE);
                MPI_Intercomm_merge(newComm, 1, &mergedComm);
                MPI_Comm_free(&comm);
                MPI_Comm_free(&newComm);
                comm = mergedComm;

                // MPI_Comm_rank(comm, &rank);
                // MPI_Comm_size(comm, &commSize);
                // printf("Rank %d received a synchronization message from the coordinator process. New communicator has %d processes.\n", rank, commSize);
                break;
            }
            case REMOVE_TAG:
            {
                // Remove the specified rank from the communicator
                MPI_Group group;
                MPI_Comm_group(comm, &group);
                MPI_Group newGroup;
                int ranks_to_exclude[1] = {message};
                MPI_Group_excl(group, 1, ranks_to_exclude, &newGroup);
                MPI_Comm newComm;
                MPI_Comm_create(comm, newGroup, &newComm);
                MPI_Group_free(&group);
                MPI_Group_free(&newGroup);
                MPI_Comm_free(&comm);
                comm = newComm;

                // If this rank is the one to be removed, terminate the process
                if (message == rank)
                {
                    printf("Rank %d terminating...\n", rank);
                    return;
                }
                else
                {
                    int newRank;
                    int newSize;
                    MPI_Comm_rank(comm, &newRank);
                    MPI_Comm_size(comm, &newSize);
                    printf("Rank %d removed the rank %d from communicator. (New rank: %d, New size: %d)\n", rank, message, newRank, newSize);
                    rank = newRank;
                    commSize = newSize;
                    break;
                }
            }
            case TERMINATION_TAG:
            {
                // Terminate the child process
                printf("Rank %d received a termination message from the coordinator process. Terminating...\n", rank);
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