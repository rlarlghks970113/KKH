#include <stdio.h>
#include <stdlib.h>
#include <math.h>

struct HeapStruct
{
	int capacity;
	int size;
	int *element;
};
typedef struct HeapStruct Heap;


//Lab 7
Heap* CreateHeap(int heapSize);
void Insert(Heap* heap, int value);
int Find(Heap* heap, int value);

//HW 7

int DeleteMax(Heap* heap);
void PrintMax(Heap* heap);

void main(int argc, char *argv[])
{
	FILE *fi = fopen(argv[1], "r");
	char cv;
	Heap* maxHeap;
	int heapSize, key;

	while (!feof(fi))
	{
		fscanf(fi, "%c", &cv);
		switch (cv)
		{
		case 'n':
			fscanf(fi, "%d", &heapSize);
			maxHeap = CreateHeap(heapSize);
			break;
		case 'i':
			
			fscanf(fi, "%d", &key);
			Insert(maxHeap, key);
			break;
		case 'd':
			DeleteMax(maxHeap);
			break;
		case 'p':
			PrintHeap(maxHeap);
			break;
		case 'f':
			fscanf(fi, "%d", &key);
			if (Find(maxHeap, key))
			{
				printf("%d is in the heap.\n", key);
			}
			else
			{
				printf("%d is not in the heap.\n", key);
			}
			break;
		}
	}
	fclose(fi);

}

Heap* CreateHeap(int heapSize)
{
	Heap* tmpHeap = malloc(sizeof(struct HeapStruct));

	tmpHeap->capacity = heapSize;
	tmpHeap->size = 0;
	tmpHeap->element = malloc(sizeof(int)* tmpHeap->capacity);

	return tmpHeap;
}

void Insert(Heap* heap, int value)
{
	int i;
	
	if (heap == NULL)
	{
		printf("There is no heap\n");

	}
	else if(Find(heap, value))
	{
		printf("%d is already in the heap\n", value);
	}
	else if (heap->capacity <= heap->size)
	{
		printf("Insertion Error: Max Heap is full\n");
	}
	else if (heap->size == 0)
	{
		heap->element[++heap->size] == value;
		printf("insert %d\n", value);
	}
	else 
	{

		for (i = ++heap->size; heap->element[i / 2] < value && i>1; i /= 2)
		{
			heap->element[i] = heap->element[i / 2];
		}
		heap->element[i] = value;
		printf("insert %d\n", value);
	}
}
int Find(Heap* heap, int value)
{
	
	for (int i = 0; i < heap->size; i++)
	{
		if (heap->element[i] == value)
		{
			return 1;
		}
	}

	return 0;
}

int DeleteMax(Heap* heap)
{
	int i;

	int MaxElement = heap->element[1];
	int lastElement = heap->element[heap->size--];

	for (i = 1; i <= heap->size;)
	{
		if (heap->element[2 * i] > heap->element[2 * i + 1])
		{
			if (heap->element[2 * i] < lastElement)
			{
				heap[i] = lastElement;
				return MaxElement;
			}
			heap->element[i] = heap->element[2 * i];
			i = 2 * i;
		}
		else
		{
			if (heap->element[2 * i + 1] < lastElement)
			{
				heap[i] = lastElement;
				return MaxElement;
			}
			heap->element[i] = heap->element[2 * i + 1];
			i = 2 * i + 1;
		}
	}
	return MaxElement;
}

void PrintMax(Heap* heap)
{
	for (int i = 1; i <= heap->size; i++)
	{
		printf("%d  ", heap->element[i]);
	}
	printf("\n");
}
