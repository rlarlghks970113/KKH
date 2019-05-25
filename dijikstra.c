#pragma warning(disable: 4996)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
	int vertex;
	int dist;
	int prev;
} Node;

typedef struct Graph {
	int size;
	int** vertices;
	Node* nodes;
} Graph;
typedef struct Heap {
	int Capacity;
	int Size;
	Node* Element;
} Heap;

Graph* CreateGraph(int size);
void printShortestPath(Graph* g);
Heap* createMinHeap(int heapSize);
void insertToMinHeap(Heap* minHeap, int vertex, int distance);
Node deleteMin(Heap* minHeap);
void reverse(int *path);

void main(int argc, char* argv[])
{
	FILE *fi = fopen(argv[1], "r");
	Graph* g;

	int size;
	fscanf(fi, "%d\n", &size);
	g = CreateGraph(size + 1);
	char temp = 0;
	while (temp != '\n')
	{
		int node1, node2, weight;
		fscanf(fi, "%d-%d-%d", &node1, &node2, &weight);
		g->vertices[node1][node2] = weight;
		temp = fgetc(fi);
	}
	printShortestPath(g);
}

Graph* CreateGraph(int size)
{
	Graph* g = malloc(sizeof(struct Graph));

	g->size = size;
	g->nodes = malloc(sizeof(struct Node) * size);
	for (int i = 0; i < size; i++)
	{
		(g->nodes[i]).vertex = i;
		(g->nodes[i]).dist = 10000;//무한대신 10000
		(g->nodes[i]).prev = -1;
	}
	g->vertices = (int*)malloc(sizeof(int*) * size);

	for (int i = 1; i <= size; i++)
	{
		g->vertices[i] = malloc(sizeof(int) * size);
		for (int j = 1; j <= size; j++)
		{
			g->vertices[i][j] = 0;
		}
	}

	return g;
}
void printShortestPath(Graph* g)
{
	(g->nodes[1]).dist = 0;
	(g->nodes[1]).prev = -1; //undefined대신 -1
	
	
	int* S = malloc(sizeof(int) * g->size);//방문한노드들의 집합
	for (int i = 1; i < g->size; i++)
	{
		S[i] = 0;
	}
	Heap* Q = createMinHeap(g->size+10);//방문하지 않은 노드들의 집합
	for (int i = 1;i<g->size; i++)
	{
		insertToMinHeap(Q, (g->nodes[i]).vertex, (g->nodes[i]).dist);
		
	}
	
	
	while (Q->Size > 0)
	{
		int alt = 0;

	
		Node u = deleteMin(Q);

		
		
		for (int i = 1; i < g->size; i++)
		{
			if (g->vertices[u.vertex][i] > 0)
			{
				alt = g->nodes[u.vertex].dist + g->vertices[u.vertex][i];
				if (alt < g->nodes[i].dist)
				{
					g->nodes[i].dist = alt;
					
					g->nodes[i].prev = u.vertex;

					
				}
			}
		}
		S[u.vertex] = 1;
		while (Q->Size != 0)
		{
			deleteMin(Q);
		}
		for (int i = 1; i < g->size; i++)
		{
			if (S[i] == 0)
			{
				insertToMinHeap(Q, g->nodes[i].vertex, g->nodes[i].dist);
			}
		}
	}

	//10000 -> INF
	int start = 1, j, i;
	for (int find = 2; find < g->size; find++)
	{
		while (g->nodes[find].prev == -1 )
		{
			printf("Cannot reach to node %d.\n", find);
			find++;
			if (find >= g->size)
			{
				return;
			}
			
		}
		int* path , count = 0;
		path = malloc(sizeof(int) * 100);
		printf("1");
		for (int i = find; i != start; i = g->nodes[i].prev)
		{
			path[count] = i;
			count++;
		}
		path[count] = -1;
		reverse(path);

		printf(" (cost: %d)\n", g->nodes[find].dist);
		
	}

}
Heap* createMinHeap(int heapSize)
{
	Heap* heap = malloc(sizeof(struct Heap));
	heap->Size = 0;
	heap->Capacity = heapSize+1;
	heap->Element = malloc(sizeof(Node) * heapSize);

	return heap;
}
void insertToMinHeap(Heap* minHeap, int vertex, int distance)
{
	if (minHeap == NULL)
	{
		printf("There is no heap\n");
	}
	int i;
	Node newNode;
	newNode.dist = distance;
	newNode.vertex = vertex;
	newNode.prev = -1;
	for (i = ++minHeap->Size; minHeap->Element[i/2].dist > distance && i > 1; i /= 2)
	{
		minHeap->Element[i] = minHeap->Element[i / 2];
	}
	minHeap->Element[i] = newNode;

	
}
Node deleteMin(Heap* minHeap)
{
	if (minHeap->Size <= 0)
	{
		printf("Deletion Error: Min Heap is empty!\n");
		return ;
	}
	int i, child;
	Node minElement = minHeap->Element[1];

	Node lastElement = minHeap->Element[minHeap->Size--];
	

	for (i = 1; minHeap->Size >= i*2; i = child)
	{
		child = i*2;
		if (child != minHeap->Size&& minHeap->Element[child + 1].dist <= minHeap->Element[child].dist)
		{
			child++;
		}

		if (lastElement.dist > minHeap->Element[child].dist)
		{
			minHeap->Element[i] = minHeap->Element[child];
		}
		else
		{
			break;
		}
		
		
	}
	
	minHeap->Element[i] = lastElement;
	
	
	return minElement;
}

void reverse(int *path)
{
	if (*path == -1)
	{
		return;
	}
	else
	{
		reverse(path + 1);
		printf(" ->%d", *path);
	}
}