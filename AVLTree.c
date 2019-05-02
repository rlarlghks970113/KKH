#pragma warning (disable:4996)
#include <stdio.h>
#include <math.h>

struct AVLNode
{
	ElementType Element;
	AVLTree Left;
	AVLTree Right;
	int Height;
};
typedef int ElementType;
typedef struct AVLNode *AVLTree;
typedef struct AVLNode *Position;

AVLTree Insert(ElementType X, AVLTree T);
void PrintInorder(AVLTree T);
void DeleteTree(AVLTree T);
Position SingleRotateWithLeft(Position node);
Position SingleRotateWithRight(Position node);
Position DoubleRotateWithLeft(Position node);
Position DoubleRotateWithRight(Position node);
int Height(Position node);

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
	return 0;
}

AVLTree Insert(ElementType X, AVLTree T)
{
	if (T == NULL)
	{
		T = malooc(sizeof(struct AVLNode));
		T->Element = X;
		T->Left = NULL;
		T->Right = NULL;
		T->Height = 0;
	}
	else if (X < T->Element)
	{
		T->Left = Insert(X, T->Left);
		
	}
	else if (X > T->Element)
	{
		T->Right = Insert(X, T->Right);
	}
	else
	{
		printf("[Error] %d is already in the tree!\n", X);
		return NULL;
	}
}
void PrintInorder(AVLTree T)
{
	if (T->Left != NULL) PrintInorder(T->Left);
	printf("%d(%d)", T->Element, T->Height);
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

	K1->Height = max(Height(K1->Left), Height(K1->Right));
	K2->Height = max(Height(K2->Left), Height(K2->Right));

	return K2;
}
Position SingleRotateWithRight(Position node)
{
	Position K1, K2;
	K1 = node;
	K2 = node->Right;

	K1->Right = K2->Left;
	K2->Left = K1;

	K1->Height = max(Height(K1->Left), Height(K1->Right));
	K2->Height = max(Height(K2->Left), Height(K2->Right));

	return K2;
}
Position DoubleRotateWithLeft(Position node);
Position DoubleRotateWithRight(Position node);
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