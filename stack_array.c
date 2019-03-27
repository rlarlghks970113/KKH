#include <stdio.h>
#include <stdlib.h>

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
bool IsFull(Stack s);
void PrintStack(Stack s);
void Pop(Stack s);

int main(int argc, char *argv[])
{
	int capacity;
	int input = 1;

	scanf_s("%d", &capacity);

	Stack s = CreateStack(capacity);

	while (input != -1)
	{
	
	printf("type your x that you want to add\n");
	scanf_s("%d", &input);

	Push(input, s);

	PrintStack(s);
	}

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
//pop
//top