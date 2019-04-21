#include <stdio.h>
#include <stdlib.h>
#define MAX_SIZE 100

int compare(int x, int y);
int binary_Search(int* list, int n, int start, int end);

void main(int argc, char *argv[])
{
	int input;
	int *list = malloc(sizeof(int) * MAX_SIZE);
	for (int i = 0; i < MAX_SIZE; i++)
	{
		list[i] = i;
	}

	scanf_s("%d", &input);

	printf("this number in list[ %d ]\n", binary_Search(list, input, 0, MAX_SIZE -1));

	free(list);
}

int compare(int x, int y)
{
	if (x < y) return -1;
	else if (x > y) return 1;
	else return 0; // x = y
}

int binary_Search(int* list, int n, int start, int end)
{

	int middle;
	if (start <= end) {
		middle = (start + end) / 2;
		switch (compare((list[middle]), n))
		{
		case -1:
			return binary_Search(list, n, middle+1, end);
			break;
		case 0:
			return middle;
			break;
		case 1:
			return binary_Search(list, n, start, middle-1);
			break;
		}
	}

	return -1;


}