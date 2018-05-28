#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mqueue.h>
#include <sys/wait.h>

#define TRUE 1
#define FALSE 0
#define END_FLAG -1
#define EOS -27
#define PRNAME "/PR"
#define MESSAGE_LENGTH 2048

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
    // check the number of arguments
    if (argc != 3)
    {
        printf("Use: prime <n> <m>\n");
        return 1;
    }

    int primeLimit = atoi(argv[1]);
    int numberOfChild = atoi(argv[2]);

    // check the input range
    if ((primeLimit < 1000 || primeLimit > 1000000) || (numberOfChild < 1 || numberOfChild > 5))
    {
        printf("Invalid <n> or <m>\n");
        return 1;
    }

    struct mq_attr mq_attr = {0};
    mq_attr.mq_msgsize = sizeof(int);
    mq_attr.mq_maxmsg = 10;
    mq_attr.mq_flags = 0;
    mq_attr.mq_curmsgs = 0;

    mqd_t fd[numberOfChild + 1];
    int i;
    for (i = 0; i < numberOfChild + 1; i++)
    {
        char mqName[8];
        snprintf(mqName, 8, "/MQ%d", i);
        mq_unlink(mqName);

        // make the last pipe non_block
        if (i == numberOfChild)
        {
            fd[i] = mq_open(mqName, O_NONBLOCK | O_RDWR | O_CREAT, 0666, &mq_attr);
        }
        else
        {
            fd[i] = mq_open(mqName, O_RDWR | O_CREAT, 0666, &mq_attr);
        }
    }

    // create a printing pipe
    mq_unlink(PRNAME);
    mqd_t pr = mq_open(PRNAME, O_RDWR| O_CREAT, 0666, NULL);

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

                while (TRUE)
                {
                    int buf[MESSAGE_LENGTH];
                    mq_receive(fd[i], (char*)buf, MESSAGE_LENGTH * sizeof(int), NULL);
                    integerRead = buf[0];

                    // reset the state
                    if (integerRead == END_FLAG)
                    {
                        // pass the end flag to next child
                        while(mq_send(fd[i + 1], (char*) &integerRead, sizeof(int), 0) == -1);
                        isFound = FALSE;
                    }
					else if (integerRead == EOS)
					{
                        mq_send(fd[i + 1], (char*) &integerRead, sizeof(int), 0);
						mq_send(pr, (char*) &integerRead, sizeof(int), 0);
                        break;
					}
                    else
                    {
                        // first integer read is a prime number
                        if (isFound == FALSE)
                        {
                            primeFound = integerRead;
                            isFound = TRUE;

                            // send it to printing pipe
                            while(mq_send(pr, (char*) &primeFound, sizeof(int), 0) == -1);
                        }
                        else
                        {
                            // if the received number is not multiple of the prime number, send it to the next pipe
                            if (integerRead % primeFound != 0)
                            {
                                while(mq_send(fd[i + 1], (char*) &integerRead, sizeof(int), 0) == -1);
                            }
                        }
                    }
                }
                exit(0);
            }
            else  /* Printing process */
            {
                int integerToPrint;
                while(TRUE)
                {
                    int buf[MESSAGE_LENGTH];
                    mq_receive(pr, (char*)buf, MESSAGE_LENGTH * sizeof(int), NULL);

                    integerToPrint = buf[0];
					if (integerToPrint == EOS)
					{	
						break;
					}
                    printf("%d\n", integerToPrint);
                    fflush(stdout);
                }
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
                int buf[MESSAGE_LENGTH];

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

                        // send the integer to first pipe
                        while(mq_send(fd[0], (char*) &value, sizeof(int), 0) == -1);
                    }


                    if (mq_receive(fd[numberOfChild], (char*)buf, MESSAGE_LENGTH * sizeof(int), NULL) > 0)
                    {
                        integerRead = buf[0];

                        int empty = isEmpty(queue);

                        enqueue(queue, integerRead);

                        if (integerRead == END_FLAG)
                        {
                            emptyProcess = TRUE;
                        }

                        if (empty && integerRead == END_FLAG)
                        {
                            int value = EOS;

                            // send end signal to child processes
                            while(mq_send(fd[0], (char*) &value, sizeof(int), 0) == -1);
                            break;
                        }
                    }
                }

                for (i = 0; i < numberOfChild + 1; i++)
                {
                    char mqName[8];
                    snprintf(mqName, 8, "/MQ%d", i);
                    mq_unlink(mqName);
                    wait(NULL);
                }
                destructQueue(queue);
                mq_unlink(PRNAME);
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
