#pragma warning(disable: 4996)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _Queue* Queue;
typedef struct _Graph* Graph;

struct _Graph {
	int size;
	int* node;
	int** matrix;
};

struct _Queue {
	int* key;
	int first;
	int rear;
	int qsize;
	int max_queue_size;
};

//Lab
Graph CreateGraph(int* nodes, int size);
void InsertEdge(Graph G, int a, int b);
void DeleteGraph(Graph G);
//HW
void Topsort(Graph G);
Queue MakeNewQueue(int X);
int IsEmpty(Queue Q);
int IsFull(Queue Q);
int Dequeue(Queue Q);
void Enqueue(Queue Q, int X);
void DeleteQueue(Queue Q);
void MakeEmpty(Queue Q);

void main(int argc, char *argv[])
{
	int numbers[100];
	int n, i = 0, a, b;
	char str[100];
	FILE *fp = fopen(argv[1], "r");
	if (fp == NULL)
	{
		printf("There is no file : %s\n", argv[1]);
		exit(-1);
	}

	fgets(str, sizeof(str), fp);

	char* ptr = strtok(str, " ");
	while (ptr != NULL)
	{
		numbers[i] = atoi(ptr);
		i++;
		ptr = strtok(NULL, " ");
	}


	Graph graph = CreateGraph(numbers, i);
	fgets(str, sizeof(str), fp);
	ptr = strtok(str, " ");
	while (ptr != NULL)
	{

		sscanf(ptr, "%d-%d", &a, &b);
		InsertEdge(graph, a, b);
		ptr = strtok(NULL, " ");
	}

	printf("  ");
	for (int j = 0; j < graph->size; j++)
	{
		printf("%d ", graph->node[j]);
	}
	printf("\n");
	for (int i = 0; i < graph->size; i++)
	{
		printf("%d ", graph->node[i]);
		for (int j = 0; j < graph->size; j++)
		{
			printf("%d ", graph->matrix[i][j]);
		}
		printf("\n");
	}
}

Graph CreateGraph(int* nodes, int size)
{
	Graph graph = (Graph)malloc(sizeof(struct _Queue));
	graph->size = size;
	graph->node = malloc(sizeof(int)*size);
	for (int i = 0; i < size; i++)
	{
		graph->node[i] = nodes[i];
	}
	graph->matrix = (int**)malloc(sizeof(int*)*size);
	for (int i = 0; i < graph->size; i++)
	{
		graph->matrix[i] = (int*)malloc(sizeof(int)*size);
	}

	for (int i = 0; i < size; i++)
	{
		for (int j = 0; j < size; j++)
		{
			graph->matrix[i][j] = 0;
		}
	}
	return graph;
}

void InsertEdge(Graph G, int a, int b)
{
	int an, bn;
	for (int i = 0; i < G->size; i++)
	{
		if (G->node[i] == a)
		{
			an = i;
		}
		if (G->node[i] == b)
		{
			bn = i;
		}
	}
	G->matrix[an][bn] = 1;
}
void DeleteGraph(Graph G)
{
	for (int i = 0; i < G->size; i++)
	{
		free(G->matrix[i]);
	}
	free(G->matrix);
	free(G->node);
}
