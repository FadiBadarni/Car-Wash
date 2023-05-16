# Car Wash Simulation

This project simulates a car wash station with multiple washing stations. It uses various concepts of concurrent programming such as processes, shared memory and semaphores.

## Table of Contents
- [Getting Started](#getting-started)
- [Usage](#usage)
- [Implementation Details](#implementation-details)

## Getting Started

To clone this repository, run the following command in your terminal:

```
git clone git@github.com:FadiBadarni/Car-Wash.git
```
## Usage
After cloning the repository, compile the `wash.c` file:
``` gcc -o wash wash.c -lpthread -lrt ```
Then run the program:
``` ./wash numOfMachine avg_arrive_time avg_wash_time run_time ```
Where:
* numOfMachine: The number of washing stations available in the car wash facility.
* avg_arrive_time: The average time between the arrivals of cars.
* avg_wash_time: The average time taken to wash a car.
* run_time: The total runtime of the simulation.

## Implementation Details

The simulation uses the following concepts:

* **Processes**: Each car is represented by a separate process. These processes simulate the arrival of cars to the washing station and the process of washing each car. A separate monitor process is also created to monitor the washing stations. The monitor process reads from a pipe and prints out when a car enters a washing station.

* **Shared Memory**: A shared memory segment is used to store the queue of cars waiting for their turn. This queue is implemented as a struct (`SharedCarQueue`) containing the number of waiting cars, the arrival and departure times of the cars, the total number of cars washed, and statistics about the waiting times and total operation time. Access to this shared memory is synchronized using a semaphore.

* **Semaphores**: Semaphores are used to synchronize access to the shared memory and the washing stations. The `queue_mutex` semaphore is used to ensure that only one process can modify the shared memory at a time, thus preventing race conditions. The `wash_station` semaphore, which is stored in the shared memory, is used to limit the number of cars that can be washed concurrently to the number of washing stations available.

* **Exponential Distribution**: The inter-arrival times of the cars and the washing times are modeled as exponential distributions, which are common models for arrival processes in queuing systems. The `rand_exp` function is used to generate exponentially distributed random numbers.

* **Signal Handling**: The simulation can be stopped prematurely by sending a SIGINT or SIGTERM signal to the main process. This is handled by the `handle_termination` function, which sets a global flag (`stop_simulation`) that causes the main loop to terminate.

* **Clean up**: After the simulation finishes or is terminated, the program cleans up by waiting for all child processes to finish, printing out the simulation results, and cleaning up the shared memory and semaphores. The cleanup process is robust to premature termination and ensures that no resources are leaked.
