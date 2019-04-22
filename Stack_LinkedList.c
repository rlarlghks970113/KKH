#pragma warning (disable:4996)
#include <stdio.h>

typedef enum { false, true } bool;

typedef struct stack* Stack;
typedef Stack Node;
struct stack {
	int key;
	Node next;
};

Stack CreateStack(Stack s);
void PrintStack(Stack s);
void Pop(Stack s);
void Push(Stack s, int key);
bool IsEmpty(Stack s);

void main(int argc, char *argv[])
{
	char input = "";
	int n;
	Stack s = NULL;
	s = CreateStack(s);

	while (input != 'q')
	{
		scanf("%c", &input);
		getchar();
		switch (input)
		{
		case 'p'://pop
			Pop(s);
			break;
		case 'o'://push
			scanf("%d", &n);
			getchar();
			Push(s, n);
			break;

		default:
			break;
		}
		PrintStack(s);
		
	}
}

Stack CreateStack(Stack s)
{
	s = (Stack)malloc(sizeof(struct stack));
	s->key = 0;
	s->next = NULL;

	return s;
}

void PrintStack(Stack s)
{
	if (!IsEmpty(s))
	{
		Node tmp = s->next;
		while (tmp)
		{
			printf("%d  ", tmp->key);
			tmp = tmp->next;
		}
		printf("\n");
	}
	else
		printf("print error\n");
}
void Pop(Stack s)
{
	if (!IsEmpty(s))
	{
		Node tmp = s->next;

		s->next = tmp->next;
		free(tmp);
	}
	else
	{
		printf("pop error\n");
	}
}
void Push(Stack s, int key)
{

	Node newNode;
	newNode = (Node)malloc(sizeof(struct stack));
	newNode->key = key;
	newNode->next = s->next;
	s->next = newNode;
}
bool IsEmpty(Stack s)
{
	if (s->next == NULL)
	{
		return true;
	}
	return false;
}