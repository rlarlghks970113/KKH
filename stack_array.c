#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { false, true } bool;

typedef struct stack *Stack;
struct stack
{
	int max_capacity;
	int top_of_stack;
	char *array;
};

Stack CreateStack(int max_capacity);
void Push(int x, Stack s);
void Pop(Stack s);
//int Top(Stack s);
void DeleteStack(Stack s);
bool IsEmpty(Stack s);
bool IsFull(Stack s);
void PrintStack(Stack s);
int Postfix(Stack s, char input_char);


int main(int argc, char *argv[])
{
	FILE* fi = fopen(argv[1], "r");

	Stack stack;
	char input_str[101];
	int max, i = 0, a, b, result;

	fgets(input_str, 101, fi);
	max = 10;
	stack = CreateStack(max);
	//p4_1 main_code

	for (i = 0; i < strlen(input_str) && input_str[i] != '#'; i++)
	{
		Push(input_str[i], stack);
	}

	fclose(fi);
	DeleteStack(stack);

	return 0;
}

Stack CreateStack(int max_capacity)
{
	Stack s;

	s = malloc(sizeof(struct stack));

	s->max_capacity = max_capacity;
	s->top_of_stack = -1;

	s->array = malloc(sizeof(char)*max_capacity);

	return s;
}

void DeleteStack(Stack s)
{
	free(s->array);
	free(s);
}

void Push(int x, Stack s)//push x in Stack s
{
	if (IsFull(s))
	{
		printf("Stack is full.\n");
	}
	else
	{
		s->top_of_stack++;
		s->array[s->top_of_stack] = x;
		printf("%d inserted\n", x);
	}

}

bool IsFull(Stack s)
{
	if (s->max_capacity > (s->top_of_stack + 1))
	{
		return false;
	}
	else
	{
		return true;
	}
}

void PrintStack(Stack s)
{
	for (int i = 0; i <= s->top_of_stack; i++)
	{
		printf("%d\t", s->array[i]);
	}
	printf("\n");
}

void Pop(Stack s)
{
	s->array[s->top_of_stack--] = NULL;
}

