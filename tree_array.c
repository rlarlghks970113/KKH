#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct CompleteTree Tree;

struct CompleteTree {
	int Size;
	int nodeNum;
	int *Element;
};

//Lab5
Tree CreateTree(int treeSize);
void Insert(Tree tree, int value);
void printTree(Tree tree);
void printInorder(Tree tree, int index);
void freeTree(Tree tree);
//Hw5
void printPreorder(Tree tree, int index);
void printPostorder(Tree tree, int index);

void main(int argc, char *argv[])
{
	FILE *fi;
	Tree tree;
	int treeSize;
	int tempNum;

	fi = fopen(argv[1], "r");
	fscanf(fi, "%d", &treeSize);
	tree = CreateTree(treeSize);
	while (fscanf(fi, "%d", &tempNum) == 1)
	{
		Insert(tree, tempNum);
	}
	printTree(tree);
	freeTree(tree);
}

Tree CreateTree(int treeSize)
{
	Tree tempTree;

	tempTree.Size = treeSize;
	tempTree.nodeNum = 0;
	tempTree.Element = malloc(sizeof(int)*tempTree.Size);

	return tempTree;
}

void Insert(Tree tree, int value)
{
	if (tree.nodeNum >= tree.Size)
	{
		printf("Tree is full!\n");
	}
	else
	{
		tree.Element[tree.nodeNum++] = value;
	}
}

void printTree(Tree tree)
{
	int i = 0;
	printf("Preorder : ");
	printInorder(tree, i);
	printf("\n");
}
void printInorder(Tree tree, int index)
{
	printf("%d	", tree.Element[index]);
	if (tree.Element[2 * index] != NULL) { printInorder(tree, 2 * index); }
	if (tree.Element[2 * index + 1 ] != NULL) { printInorder(tree, 2 * index + 1); }
}

void freeTree(Tree tree)
{
	free(tree.Element);
}