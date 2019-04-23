#pragma warning (disable:4996)
#include <stdio.h>
#include <stdlib.h>
#define MAX_SIZE 10

typedef enum { false, true } bool;


struct QueueRecord {
	int Capacity;
	int Front;
	int Rear;
	int Size;

	int* Array;
};
typedef struct QueueRecord* Queue;

Queue MakeEmpty(Queue Q);
bool IsFull(Queue Q);
void Enqueue(int Key, Queue Q);
void Dequeue(Queue Q);
void PrintQueue(Queue Q);

void main(int argc, char *argv[])
{
	Queue a = NULL;
	a = MakeEmpty(a);
	char input = "";
	int n;
	while (input != 'q')
	{
		scanf("%c", &input);
		getchar();
		switch (input)
		{
		case 'e':
			scanf("%d", &n);
			getchar();
			Enqueue(n, a);
			break;
		case 'd':
			Dequeue(a);
			break;
		default:
			break;
		}
		PrintQueue(a);
	}


}

Queue MakeEmpty(Queue Q)
{
	Q = malloc(sizeof(struct QueueRecord));
	Q->Capacity = MAX_SIZE;
	Q->Size = 0;
	Q->Front = 0;
	Q->Rear = 0;
	Q->Array = (int)malloc(sizeof(int)*MAX_SIZE);

	return Q;
}

bool IsFull(Queue Q)
{
	return Q->Front == (Q->Rear + 1) % Q->Capacity;
}

void Enqueue(int Key, Queue Q)
{
	if (!IsFull(Q)) {

		Q->Rear = (Q->Rear + 1) % Q->Capacity;
		Q->Array[Q->Rear] = Key;
		Q->Size++;
	}
}
void Dequeue(Queue Q)
{
	if (!IsFull(Q) || Q->Size > 0) {
		Q->Front = (Q->Front + 1) % Q->Capacity;
		Q->Size--;
	}
}

void PrintQueue(Queue Q)
{
	for (int i = Q->Front + 1; i != (Q->Rear + 1) % Q->Capacity; i = (i + 1) % Q->Capacity)
	{
		printf("%d  ", Q->Array[i]);
	}
	printf("\n");
}
