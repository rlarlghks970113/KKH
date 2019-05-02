#pragma warning (disable:4996)
#include <stdio.h>
#include <math.h>
#include <stdlib.h>



typedef int ElementType;
typedef struct AVLNode* AVLTree;
typedef struct AVLNode* Position;

struct AVLNode
{
        ElementType Element;
        AVLTree Left;
        AVLTree Right;
        int Height;
};


AVLTree Insert(ElementType X, AVLTree T);
void PrintInorder(AVLTree T);
void DeleteTree(AVLTree T);
Position SingleRotateWithLeft(Position node);
Position SingleRotateWithRight(Position node);
Position DoubleRotateWithLeft(Position node);
Position DoubleRotateWithRight(Position node);
int Height(Position node);
int max(int a, int b)
{
	if(a>b)
	{
		return a;
	}
	else
	{
		return b;
	}
}

void main(int argc, char *argv[])
{
	FILE *fp = fopen(argv[1], "r");
	AVLTree myTree = NULL;
	int num;

	if (fp == NULL)
	{
		printf("There is no file : %s\n", argv[1]);
		exit(-1);
	}

	while (fscanf(fp, "%d", &num) != EOF)
	{
		myTree = Insert(num, myTree);
		PrintInorder(myTree);
		printf("\n");
	}

	DeleteTree(myTree);
}

AVLTree Insert(ElementType X, AVLTree T)
{
	if (T == NULL)
	{
		T = malloc(sizeof(struct AVLNode));
		T->Element = X;
		T->Left = NULL;
		T->Right = NULL;
		T->Height = 0;
	}
	else if (X < T->Element)
	{
		T->Left = Insert(X, T->Left);
		if(Height(T->Left) - Height(T->Right) == 2)
		{	
			if(X < T->Left->Element)
			{
				T = SingleRotateWithLeft(T);
			}
			else
			{
				T = DoubleRotateWithLeft(T);
			}
		}
	}
	else if (X > T->Element)
	{
		T->Right = Insert(X, T->Right);
		if(Height(T->Right) - Height(T->Left) == 2)
		{
			if(X > T->Right->Element)
			{
				T = SingleRotateWithRight(T);
			}
			else
			{
				T = DoubleRotateWithRight(T);
			}
		}
	}
	else if (X == T->Element)
	{
		printf("[Error] %d is already in the tree!\n", X);
	}
	
	
	T->Height = max(Height(T->Left), Height(T->Right)) +1;
	
	return T;
}
void PrintInorder(AVLTree T)
{
	if (T->Left != NULL) PrintInorder(T->Left);
	printf("%d(%d) ", T->Element, T->Height);
	if (T->Right != NULL) PrintInorder(T->Right);
}
void DeleteTree(AVLTree T)
{
	if (T->Left != NULL) DeleteTree(T->Left);
	if (T->Right != NULL) DeleteTree(T->Right);
	free(T);
}
Position SingleRotateWithLeft(Position node)
{
	Position K1, K2;
	K1 = node;
	K2 = node->Left;

	K1->Left = K2->Right;
	K2->Right = K1;

	K1->Height = max(Height(K1->Left), Height(K1->Right)) + 1;
	K2->Height = max(Height(K2->Left), Height(K2->Right)) + 1;

	return K2;
}
Position SingleRotateWithRight(Position node)
{
	Position K1, K2;
	K1 = node;
	K2 = node->Right;

	K1->Right = K2->Left;
	K2->Left = K1;

	K1->Height = max(Height(K1->Left), Height(K1->Right)) +1;
	K2->Height = max(Height(K2->Left), Height(K2->Right)) +1;

	return K2;
}
Position DoubleRotateWithLeft(Position node)
{
	node->Left = SingleRotateWithRight(node->Left);

	return SingleRotateWithLeft(node);
}
Position DoubleRotateWithRight(Position node)
{
	node->Right = SingleRotateWithLeft(node->Right);

	return SingleRotateWithRight(node);
}
int Height(Position node)
{
	if (node == NULL)
	{
		return -1;
	}
	else
	{
		return node->Height;
	}
}
