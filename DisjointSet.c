#pragma warning (disable:4996)

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

typedef struct _DisjointSet
{
	int size_maze;
	int *ptr_arr;

} DisjointSets;

typedef struct _Maze_print
{
	int *right_wall;// l
	int *down_wall;// _
} Maze_print;

void init(DisjointSets *sets, Maze_print *maze_print, int num);
int find(DisjointSets *sets, int i);
void unionA(DisjointSets *sets, int i, int j);


void printMaze(Maze_print *sets, int num);
void freeMaze(DisjointSets *sets, Maze_print *maze_print);
bool isVisitAll(int *array, int i, int size);//방문했는지 체크
bool isWall(int* visited, int i, int num, int beforex);//벽이나 이미 방문했는지 체크
void createMaze(DisjointSets *sets, Maze_print *maze_print, int num);
bool isPossible(DisjointSets *sets, int *visited, int x, int num);

void main(int argc, char *argv[])
{
	srand((unsigned int)time(NULL));

	int num, i;
	FILE *fi = fopen(argv[1], "r");
	DisjointSets *sets;
	Maze_print *maze_print;
	fscanf(fi, "%d", &num);
	sets = (DisjointSets*)malloc(sizeof(DisjointSets));
	maze_print = (DisjointSets*)malloc(sizeof(DisjointSets));
	init(sets, maze_print, num);
	createMaze(sets, maze_print, num);
	printMaze(maze_print, num);
	freeMaze(sets, maze_print);

}

void init(DisjointSets *sets, Maze_print *maze_print, int num)
{
	sets->size_maze = num * num;
	sets->ptr_arr = malloc(sizeof(int) * (sets->size_maze + 1));
	maze_print->right_wall = malloc(sizeof(int) *(sets->size_maze + 1));
	maze_print->down_wall = malloc(sizeof(int) *(sets->size_maze + 1));

	for (int i = 1; i <= sets->size_maze; i++)
	{
		sets->ptr_arr[i] = 0;
		maze_print->right_wall[i] = 1;
		maze_print->down_wall[i] = 1; //벽 있는 것
	}
}

int find(DisjointSets *sets, int i)
{
	while (sets->ptr_arr[i] > 0)
	{
		i = sets->ptr_arr[i];
	}

	return i;
}




void createMaze(DisjointSets *sets, Maze_print *maze_print, int num) {
	int x, direction, afterx=0;
	int maze_size = num * num + 1;//미로 사이즈

	int *visited = malloc(sizeof(int) * maze_size);//방문여부 0 = 미방문, 1 = 방문
	for (int i = 1; i < maze_size; i++)
	{
		visited[i] = 0;
	}
	do
	{
		do
		{
			x = rand() % (num*num) + 1;//랜덤한 X값 설정
		} while (visited[x] == 1);
		visited[x] = 1;

		do
		{
			if (!isPossible(sets, visited, x, num)) break;

			do
			{
				
				direction = rand() % 4 + 1;
				if (direction == 1)	afterx = x - num;
				else if (direction == 2) afterx = x + 1;
				else if (direction == 3) afterx = x + num;
				else if (direction == 4) afterx = x - 1;

				if (!isWall(visited, afterx, num, x))
				{
					if (find(sets, x) != find(sets, afterx))
					{
						break;
					}
				}
			} while (true);


			unionA(sets, afterx, x);
			
			int temp = x;
			x = afterx;
			
			visited[x] = 1;
			
			if (direction == 1)
			{
				maze_print->down_wall[x] = 0;
			}
			else if (direction == 2)
			{
				maze_print->right_wall[x - 1] = 0;
			}
			else if (direction == 3)
			{
				maze_print->down_wall[x - num] = 0;
			}
			else if (direction == 4)
			{
				maze_print->right_wall[x] = 0;
			}

			
		} while (true);
	} while (!isVisitAll(visited, x, maze_size));

	maze_print->right_wall[num*num] = 0;
	free(visited);
}

void printMaze(Maze_print *sets, int num)
{
	for (int i = 1; i <= num; i++)
	{
		printf(" _");
	}
	printf("\n");
	int i = 0, j = 0;

	for (i = 0; i < num; i++)
	{
		if (i == 0 && j == 0)
		{
			printf(" ");
		}
		else
		{
			printf("|");
		}
		
		for (int j = 1; j <= num; j++)
		{
			if (sets->down_wall[j + i * num] == 1)
			{
				printf("_");
			}
			else if (sets->down_wall[j + i * num] == 0)
			{
				printf(" ");
			}

			if (sets->right_wall[j + i * num] == 1)
			{
				printf("|");
			}
			else if(sets->right_wall[j + i * num] == 0)
			{
				printf(" ");
			}

			
		}
		printf(" \n");
	}
	printf("\n");
}
void freeMaze(DisjointSets *sets, Maze_print *maze_print) {
	free(sets->ptr_arr);
	free(maze_print->down_wall);
	free(maze_print->right_wall);
	free(sets);
	free(maze_print);
}
bool isVisitAll(int* array, int i, int size)//방문했는지 체크
{
	for (int i = 1; i <= size; i++)
	{
		if (array[i] == 0)
		{
			return false;
		}
	}
	return true;
}
bool isWall(int* visited, int i, int num, int beforex) {

	if (i < 1) return true; //위 없음
	else if (i >num *num ) return true; //아래 없음
	else if (i%num == 0 && beforex == i+1) return true; //왼쪽;
	else if (i == beforex +1 && beforex % num == 0) return true; //오른쪽 없음


	return false;
}

void unionA(DisjointSets *sets, int i, int j)
{
	int r1 = find(sets, i);
	int r2 = find(sets, j);

	if (sets->ptr_arr[r1] < sets->ptr_arr[r2])
	{
		sets->ptr_arr[r2] = r1;
	}
	else
	{
		if (sets->ptr_arr[r1] == sets->ptr_arr[r2])
		{
			(sets->ptr_arr[r2])--;
		}
		sets->ptr_arr[r1] = r2;
	}
}

bool isPossible(DisjointSets *sets, int *visited, int x, int num)
{
	bool up = false, down = false, right = false, left = false;
	int afterx;

	afterx = x - num;
	if (!isWall(visited, afterx, num, x))
	{
		if (find(sets, afterx) != find(sets, x))
		{
			up = true;
		}
	}

	afterx = x + 1;
	if (!isWall(visited, afterx, num, x))
	{
		if (find(sets, afterx) != find(sets, x))
		{
			right = true;
		}
	}

	afterx = x + num;
	if (!isWall(visited, afterx, num, x))
	{
		if (find(sets, afterx) != find(sets, x))
		{
			down = true;
		}
	}

	afterx = x - 1;
	if (!isWall(visited, afterx, num, x))
	{
		if (find(sets, afterx) != find(sets, x))
		{
			left = true;
		}
	}

	if (!up && !down && !right&&!left)
	{
		return false;
	}
	else
	{
		return true;
	}
}