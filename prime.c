#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>

#define READ_END 0
#define WRITE_END 1
#define END_FLAG -1
#define EOS -27
#define TRUE 1
#define FALSE 0

// node structure for the queue
typedef struct Node {
    int data;
    struct Node *next;
} Node;

// queue structure
typedef struct Queue {
    Node *head;
    Node *tail;
    int size;
} Queue;

// queue functions
Queue *constructEmptyQueue();
void destructQueue(Queue *queue);
void enqueue(Queue *queue, int data);
int dequeue(Queue *queue);
int isEmpty(Queue* queue);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Use: prime <n> <m>\n");
        return 1;
    }

    int primeLimit = atoi(argv[1]);
    int numberOfChild = atoi(argv[2]);

    if ((primeLimit < 1000 || primeLimit > 1000000) || (numberOfChild < 1 || numberOfChild > 50))
    {
        printf("Invalid <n> or <m>\n");
        return 1;
    }
    int i;

    // creating printing pipe
    int printingPipe[2];
    if (pipe(printingPipe) < 0)
    {
        printf("Could not create pipe\n");
        exit(1);
    }

    // creating child process pipes
    int fd[numberOfChild + 1][2];
    for (i = 0; i < numberOfChild + 1; i++)
    {
        if (pipe(fd[i]) < 0)
        {
            printf("Could not create pipe\n");
            exit(1);
        }
    }

    // make the last pipe non-blocking
    fcntl(fd[numberOfChild][READ_END], F_SETFL, O_NONBLOCK);

    // create a queue and fill it
    Queue* queue = constructEmptyQueue();
    for (i = 2; i <= primeLimit; i++)
    {
        enqueue(queue, i);
    }
    enqueue(queue, END_FLAG);


    for (i = 0; i < numberOfChild + 1; i++)
    {
        // forking
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("Fork error\n");
            return 1;
        }
        else if (pid == 0) /* Child process */
        {
            if (i != numberOfChild)
            {
                // initialize state of the child process
                int isFound = FALSE;
                int primeFound = 0;
                int integerRead = 0;

                // read the pipe content
                close (fd[i][WRITE_END]);
                close(printingPipe[READ_END]);
                close(fd[i + 1][READ_END]);
                while (TRUE)
                {
                    read(fd[i][READ_END], &integerRead, sizeof(integerRead));

                    // reset the state
                    if (integerRead == END_FLAG)
                    {
                        write(fd[i + 1][WRITE_END], &integerRead, sizeof(integerRead));
                        isFound = FALSE;
                    }
					else if (integerRead == EOS)
					{
                        write(fd[i + 1][WRITE_END], &integerRead, sizeof(integerRead));
						write(printingPipe[WRITE_END], &integerRead, sizeof(integerRead));
                        break;
					}
                    else
                    {
                        // first integer read is a prime number
                        if (isFound == FALSE)
                        {
                            primeFound = integerRead;
                            isFound = TRUE;

                            // send it to the printing pipe
                            write(printingPipe[WRITE_END], &primeFound, sizeof(primeFound));
                        }
                        else
                        {
                            // if the received number is not multiple of the prime number, send it to the next pipe
                            if (integerRead % primeFound != 0)
                            {
                                write(fd[i + 1][WRITE_END], &integerRead, sizeof(integerRead));
                            }
                        }
                    }
                }
                close(fd[i + 1][WRITE_END]);
                close(fd[i][READ_END]);
                close(printingPipe[WRITE_END]);
                exit(0);
            }
            else  /* Printing process */
            {
                int integerToPrint;

                close(printingPipe[WRITE_END]);
                while(TRUE)
                {
                    read(printingPipe[READ_END], &integerToPrint, sizeof(integerToPrint));
					if (integerToPrint == EOS)
					{	
						break;
					}
                    printf("%d\n", integerToPrint);
                    fflush(stdout);
                }
                close(printingPipe[READ_END]);
				exit(0);
            }
        }
        else /* Parent process */
        {
            // if the last child has created, start generating numbers
            if (i == numberOfChild)
            {
                int integerRead;
                int emptyProcess = TRUE;

                close(fd[0][READ_END]);
                close(fd[numberOfChild][WRITE_END]);
                while (TRUE)
                {
                    // when the end flag has added to the queue, start dequeue the integers
                    if (emptyProcess)
                    {
                        int value = dequeue(queue);

                        // when the end flag has dequeued, start filling the queue again
                        if (value == END_FLAG)
                        {
                            emptyProcess = FALSE;
                        }

                        write(fd[0][WRITE_END], &value, sizeof(value));
                    }

                    if (read(fd[numberOfChild][READ_END], &integerRead, sizeof(integerRead)) > 0)
                    {
                        int empty = isEmpty(queue);

                        enqueue(queue, integerRead);

                        if (integerRead == END_FLAG)
                        {
                            emptyProcess = TRUE;
                        }

                        if (empty && integerRead == END_FLAG)
                        {
							int value = EOS;
							write(fd[0][WRITE_END], &value, sizeof(value));
                            destructQueue(queue);
                            break;
                        }
                    }
                }
				for (i = 0; i < numberOfChild + 1; i++)
					wait(NULL);
                close(fd[0][WRITE_END]);
                close(fd[numberOfChild][READ_END]);
            }
        }
    }
    return 0;
}

/* queue implementation below */

// constructor
Queue *constructEmptyQueue()
{
    // creating an empty queue
    Queue *queue = (Queue*) malloc(sizeof (Queue));

    if (queue == NULL)
    {
        return NULL;
    }

    // setting attributes of the queue
    queue->size = 0;
    queue->head = NULL;
    queue->tail = NULL;

    return queue;
}

// destructor
void destructQueue(Queue *queue)
{
    while (!isEmpty(queue))
    {
        dequeue(queue);
    }
    free(queue);
}

void enqueue(Queue *queue, int data)
{
    if (queue != NULL)
    {
        Node *newNode = (Node*) malloc(sizeof (Node));
        newNode->data = data;
        newNode->next = NULL;

        if (isEmpty(queue))
        {
            queue->head = newNode;
            queue->tail = newNode;
        }
        else
        {
            (queue->tail)->next = newNode;
            queue->tail = newNode;
        }
        queue->size = queue->size + 1;
    }
}

int dequeue(Queue *queue)
{
    int returnedData = END_FLAG;

    if (!isEmpty(queue))
    {
        Node* nodeToDelete = queue->head;
        queue->head = (queue->head)->next;
        returnedData = nodeToDelete->data;
        free(nodeToDelete);
        queue->size = queue->size - 1;
    }

    return returnedData;
}

int isEmpty(Queue* queue)
{
    if (queue == NULL)
    {
        return TRUE;
    }
    if (queue->size == 0)
    {
        return TRUE;
    }
    return FALSE;
}
