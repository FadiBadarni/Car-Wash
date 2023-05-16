#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>

#define MAX_WASHING_STATIONS 10
#define SHARED_MEM_NAME "/car_queue"

sem_t queue_mutex; // semaphore for the shared memory access
pid_t main_pid;

int pipefd[2]; // Declare pipe file descriptor array
int car_ids[MAX_WASHING_STATIONS];

typedef struct
{
    int waiting_cars;
    double arrival_time;
    double departure_time;
    int cars_washed;
    double total_waiting_time;
    double total_time;
    sem_t wash_station;
} SharedCarQueue;

int avg_arrive_time, avg_wash_time;
int active_processes = 0;
double start_time;

volatile sig_atomic_t stop_simulation = 0;

double rand_exp(double lambda)
{
    double u = rand() / (RAND_MAX + 1.0);
    return -log(1 - u) / lambda;
}

void handle_termination(int sig)
{
    stop_simulation = 1;
}

void monitor_washing()
{
    while (1)
    {
        int car_id;
        if (read(pipefd[0], &car_id, sizeof(car_id)) > 0)
        {
        }
        if (stop_simulation)
        {
            break;
        }
    }
    exit(0);
}

void *car_wash(void *arg)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double arrival_time = tv.tv_sec + tv.tv_usec / 1e6;
    arrival_time -= start_time; // Subtract start time to get time relative to start of simulation
    int car_id = *((int *)arg);
    printf("\033[0;32m>>> Car %d arrived at facility at time %.6lf <<<\033[0m\n", car_id, arrival_time);

    // open shared memory
    int shm_fd = shm_open(SHARED_MEM_NAME, O_RDWR, 0666);
    SharedCarQueue *shared_queue = (SharedCarQueue *)mmap(0, sizeof(SharedCarQueue), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // increment the waiting cars count
    sem_wait(&queue_mutex);
    shared_queue->waiting_cars++;
    shared_queue->arrival_time = arrival_time;
    sem_post(&queue_mutex);
    active_processes++;

    // wait until a washing station is available
    sem_wait(&shared_queue->wash_station);

    // Send a message through the pipe indicating a car has entered a washing station
    write(pipefd[1], &car_id, sizeof(car_id));

    // decrement the waiting cars count
    sem_wait(&queue_mutex);
    shared_queue->waiting_cars--;
    sem_post(&queue_mutex);
    active_processes--;

    gettimeofday(&tv, NULL);
    double entry_time = tv.tv_sec + tv.tv_usec / 1e6;
    entry_time -= start_time; // Subtract start time to get time relative to start of simulation
    printf("\033[0;33m>>> Car %d entered a washing station at time %.6lf <<<\033[0m\n", car_id, entry_time);

    // sleep to simulate the time taken to wash the car
    usleep(rand_exp(1.0 / avg_wash_time) * 1e6); // sleep to simulate the time taken to wash the car

    gettimeofday(&tv, NULL);
    double departure_time = tv.tv_sec + tv.tv_usec / 1e6;
    departure_time -= start_time; // Subtract start time to get time relative to start of simulation
    printf("\033[0;31m>>> Car %d has finished washing and left the facility at time %.6lf <<<\033[0m\n", car_id, departure_time);
    sem_post(&shared_queue->wash_station); // signal that a washing station is now available

    double waiting_time = departure_time - arrival_time; // Calculate waiting time for this car

    // increment the cars washed count and update total waiting time
    sem_wait(&queue_mutex);
    shared_queue->cars_washed++;
    shared_queue->total_waiting_time += waiting_time;
    sem_post(&queue_mutex);

    // update the departure time
    sem_wait(&queue_mutex);
    shared_queue->departure_time = departure_time;
    sem_post(&queue_mutex);

    // update the total time
    sem_wait(&queue_mutex);
    shared_queue->total_time = departure_time;
    sem_post(&queue_mutex);

    // unmap and close shared memory
    munmap(shared_queue, sizeof(SharedCarQueue));
    close(shm_fd);

    exit(0); // Always exit the child process after a car is washed

    return NULL;
}

int main(int argc, char *argv[])
{
    main_pid = getpid();
    if (argc != 5)
    {
        fprintf(stderr, "Usage: ./wash numOfMachine avg_arrive_time avg_wash_time run_time\n");
        exit(EXIT_FAILURE);
    }

    int num_washing_stations = atoi(argv[1]);
    avg_arrive_time = atoi(argv[2]);
    avg_wash_time = atoi(argv[3]);
    int simulation_time = atoi(argv[4]);

    signal(SIGTERM, handle_termination);
    signal(SIGINT, handle_termination);

    // initialize the semaphores
    sem_init(&queue_mutex, 0, 1);

    // create and initialize shared memory
    int shm_fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(SharedCarQueue));
    SharedCarQueue *shared_queue = (SharedCarQueue *)mmap(0, sizeof(SharedCarQueue), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    shared_queue->waiting_cars = 0;
    shared_queue->cars_washed = 0;
    shared_queue->total_waiting_time = 0.0;
    shared_queue->total_time = 0.0;
    sem_init(&shared_queue->wash_station, 1, num_washing_stations); // initialize the semaphore in shared memory

    // Create a pipe
    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Create a new process to monitor the washing stations
    pid_t monitor_pid = fork();

    if (monitor_pid == 0)
    {
        // This is the child process. Call the monitor_washing function and then exit.
        monitor_washing();
        exit(0);
    }
    else if (monitor_pid < 0)
    {
        // Fork failed
        perror("fork");
        return 1;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    start_time = tv.tv_sec + tv.tv_usec / 1e6;
    double current_time = start_time;
    int i = 0;

    pid_t pids[MAX_WASHING_STATIONS];

    while ((current_time - start_time) < simulation_time && !stop_simulation && active_processes < num_washing_stations)
    {
        car_ids[i % MAX_WASHING_STATIONS] = i;
        // Create a new process instead of a thread
        pids[i % MAX_WASHING_STATIONS] = fork();

        if (pids[i % MAX_WASHING_STATIONS] == 0)
        {
            // This is the child process. Call the car_wash function and then exit.
            car_wash(&car_ids[i % MAX_WASHING_STATIONS]);
            exit(0);
        }
        else if (pids[i % MAX_WASHING_STATIONS] < 0)
        {
            // Fork failed
            perror("fork");
            return 1;
        }

        usleep(rand_exp(1.0 / avg_arrive_time) * 1e6); // simulate time between car arrivals
        i++;
        gettimeofday(&tv, NULL);
        current_time = tv.tv_sec + tv.tv_usec / 1e6;
    }

    // keep washing remaining cars until the queue is empty
    while (shared_queue->waiting_cars > 0 && !stop_simulation)
    {
        /* As there are still cars waiting, there's no need to create new processes
        just wait for the remaining processes to finish */
        usleep(1000);
    }

    // Wait for all active processes to finish before proceeding
    for (int j = 0; j < i; j++)
    {
        int status;
        while (waitpid(pids[j % MAX_WASHING_STATIONS], &status, WNOHANG) == 0)
        {
            usleep(1000); // sleep for a short period of time to reduce CPU usage
        }
    }

    for (int j = 0; j < i; j++)
    {
        // Replace pthread_join with waitpid to wait for the child processes
        int status;
        waitpid(pids[j % MAX_WASHING_STATIONS], &status, 0);
        if (WIFEXITED(status))
        {
            const int exit_status = WEXITSTATUS(status);
        }
    }

    double average_waiting_time = shared_queue->total_waiting_time / shared_queue->cars_washed;
    gettimeofday(&tv, NULL);
    double end_time = tv.tv_sec + tv.tv_usec / 1e6;

    printf("\n********** CAR WASH SIMULATION RESULTS **********\n");
    printf("\nTotal cars that experienced our excellent service: %d cars\n", shared_queue->cars_washed);
    printf("Average waiting time: %.2f seconds\n", average_waiting_time);
    printf("Total time for the simulation: %.2f seconds\n", end_time - start_time);
    printf("\n*************************************************\n");

    stop_simulation = 1;        // signal to the monitor process to stop
    kill(monitor_pid, SIGTERM); // send SIGTERM to the monitor process
    int status;
    waitpid(monitor_pid, &status, 0); // wait for the monitor process to finish

    // Cleanup - Unlink and close the shared memory
    munmap(shared_queue, sizeof(SharedCarQueue));
    close(shm_fd);
    shm_unlink(SHARED_MEM_NAME);

    // Cleanup - Destroy the semaphores
    if (getpid() == main_pid)
    {
        sem_destroy(&shared_queue->wash_station);
        sem_destroy(&queue_mutex);
    }

    sem_destroy(&queue_mutex);

    printf("Parent process %d terminating\n", getpid());
    return 0;
}
