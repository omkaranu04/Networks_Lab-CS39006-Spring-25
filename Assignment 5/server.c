/*
=====================================
Assignment 5
Server Implementation
Name: Omkar Bhandare
Roll number: 22CS30016
=====================================
*/
// ASSUME THAT EACH CLIENT REQUESTS ONLY FOR 20 TASKS
// ASSUME THAT ALL THE TASKS ARE MATHEMATICALLY COREECT, NO CHECKS HAVE BEEN IMPLEMENTED
// ASSUMING THAT THE SERVER NEVER FAILS TO SEND OR ACKNOWLEDGE ANY KIND OF MESSAGE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <semaphore.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_TASKS 200
#define MAX_CLIENTS 20
#define TIMEOUT_ATTEMPTS 5

typedef struct
{
    char tasks[MAX_TASKS][50];
    int task_status[MAX_TASKS]; // 0: available, 1: assigned, 2: completed
    int assigned_to[MAX_TASKS]; // client PID
    int num_tasks;
} TaskQueue;

int shmid;
TaskQueue *task_queue;
sem_t *mutex;

void load_tasks(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }

    task_queue->num_tasks = 0;

    char line[50];
    while (fgets(line, 50, file) && task_queue->num_tasks < MAX_TASKS)
    {
        line[strcspn(line, "\n")] = 0;
        strcpy(task_queue->tasks[task_queue->num_tasks], line);
        task_queue->task_status[task_queue->num_tasks] = 0;
        task_queue->assigned_to[task_queue->num_tasks] = 0;
        task_queue->num_tasks++;
    }

    fclose(file);
    printf("Loaded %d tasks from %s\n", task_queue->num_tasks, filename);
}

void reaasign_lost_tasks()
{
    sem_wait(mutex);
    for (int i = 0; i < task_queue->num_tasks; i++)
    {
        if (task_queue->task_status[i] == 1)
        {
            pid_t assigned_pid = task_queue->assigned_to[i];

            if (kill(assigned_pid, 0) == -1 && errno == ESRCH)
            {
                printf("Client %d died before completing this task %d. Reassigning...\n", assigned_pid, i + 1);
                task_queue->task_status[i] = 0;
                task_queue->assigned_to[i] = 0;
            }
        }
    }

    sem_post(mutex);
}

int get_next_task(int client_pid)
{
    reaasign_lost_tasks();

    sem_wait(mutex);
    for (int i = 0; i < task_queue->num_tasks; i++)
    {
        if (task_queue->task_status[i] == 0)
        {
            task_queue->task_status[i] = 1; // task assigned
            task_queue->assigned_to[i] = client_pid;
            sem_post(mutex);
            return i;
        }
    }
    sem_post(mutex);
    return -1;
}

void send_message(int client_socket, const char *message)
{
    char temp[BUFFER_SIZE];
    snprintf(temp, BUFFER_SIZE, "%s\n", message);
    send(client_socket, temp, strlen(temp), 0);
}

void handle_client(int client_socket, struct sockaddr_in client_addr)
{
    char buffer[BUFFER_SIZE];
    int client_pid = getpid();
    int client_task = -1, cnt = 0;

    printf("S: client connected ---(PID: %d)\n", client_pid);

    fcntl(client_socket, F_SETFL, O_NONBLOCK);

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(client_socket, buffer, BUFFER_SIZE, 0);

        if (n > 0)
        {
            buffer[strcspn(buffer, "\n")] = '\0';
            printf("S: %d: %s\n", client_pid, buffer);
            cnt = 0; // timeout reset

            if (strcmp(buffer, "GET_TASK") == 0)
            {
                if (client_task != -1)
                {
                    send_message(client_socket, "Complete your current task first");
                    continue;
                }
                client_task = get_next_task(client_pid);
                if (client_task != -1)
                {
                    char task_buff[BUFFER_SIZE];
                    snprintf(task_buff, BUFFER_SIZE, "Task: %s", task_queue->tasks[client_task]);
                    send_message(client_socket, task_buff);
                    printf("S: sent task %d to client %d: %s\n", client_task + 1, client_pid, task_queue->tasks[client_task]);
                }
                else
                {
                    send_message(client_socket, "No tasks available");
                }
            }
            else if (strncmp(buffer, "RESULT ", 7) == 0)
            {
                if (client_task != -1)
                {
                    sem_wait(mutex);
                    task_queue->task_status[client_task] = 2; // task completed
                    sem_post(mutex);
                    printf("S: client %d completed task %d\n", client_pid, client_task + 1);
                    client_task = -1;
                }
                send_message(client_socket, "Result received");
            }
            else if (strcmp(buffer, "exit") == 0)
            {
                printf("S: client exited ---(PID: %d)\n", client_pid);
                close(client_socket);
                exit(0);
            }
            else
            {
                send_message(client_socket, "Unknown command");
            }
        }
        else if (n == 0)
        {
            printf("S: client disconnected ---(PID: %d)\n", client_pid);
            close(client_socket);
            exit(0);
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            perror("recv failed");
            close(client_socket);
            exit(EXIT_FAILURE);
        }
        else
        {
            cnt++;
            if (cnt >= TIMEOUT_ATTEMPTS)
            {
                printf("S: client timeout, exiting ---(PID: %d)\n", client_pid);
                close(client_socket);
                exit(EXIT_FAILURE);
            }
        }
        sleep(1);
    }
}
void handler(int signum)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}
int main(int argc, char const *argv[])
{

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <task_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    key_t key = ftok(".", 'A');
    shmid = shmget(key, sizeof(TaskQueue), 0666 | IPC_CREAT);
    if (shmid < 0)
    {
        perror("Shared memory creation failed");
        exit(EXIT_FAILURE);
    }

    task_queue = (TaskQueue *)shmat(shmid, NULL, 0);
    if (task_queue == (void *)-1)
    {
        perror("Shared memory attachment failed");
        exit(EXIT_FAILURE);
    }

    mutex = sem_open(".", O_CREAT, 0666, 1);

    load_tasks(argv[1]);

    signal(SIGCHLD, handler);

    int server_fd, client_socket;
    struct sockaddr_in address, client_addr;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Task Queue Server started on port %d\n", PORT);
    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    while (1)
    {
        client_socket = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);
        if (client_socket >= 0)
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                close(server_fd);
                handle_client(client_socket, client_addr);
                exit(0);
            }
            else
            {
                close(client_socket);
            }
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            perror("accept failed");
        }

        usleep(100000);
    }

    shmdt(task_queue);
    shmctl(shmid, IPC_RMID, NULL);
    sem_close(mutex);
    sem_unlink(".");

    return 0;
}
