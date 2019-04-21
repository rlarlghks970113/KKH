#include <stdio.h>
#include <math.h>
#include <time.h>
#define MAX_SIZE 400
#define SWAP(x, y,t) ((t)=(x), (x)=(y), (y)=(t))

void sort(int* list, int n);
void printStar(int n);

void main(int argc, char *argv[])
{
	int list[MAX_SIZE];
	srand(time(NULL));
	for (int i = 0; i < MAX_SIZE; i++)
	{

		list[i] = rand() % 90 + 1;
		printStar(list[i]);
	}


	sort(list, MAX_SIZE);

	printf("\nresult :\n");
	for (int i = 0; i < MAX_SIZE; i++)
	{
		printStar(list[i]);
	}
}

void sort(int* list, int n)
{
	int i, j, temp; //sorti는 정렬시작 하는 배열 인덱스
	int min = 0;
	for (i = 0; i < MAX_SIZE; i++)
	{
		min = i;
		for (j = i+1; j < MAX_SIZE; j++)
		{
			if(list[min] > list[j])
			{
				min = j;
			}
		}
		SWAP(list[min], list[i], temp);
	}
}

void printStar(int n)
{
	for (int i = 0; i < n; i++)
	{
		printf("*");
	}
	printf("\n");
}