#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct BinarySearchTree* Tree;
struct BinarySearchTree
{
	int value;
	Tree left;
	Tree right;
};

//Lab6
Tree insertNode(Tree root, int key);
void findNode(Tree root, int key);
void printInorder(Tree root);
void deleteTree(Tree root);

//HW6
Tree deleteNode(Tree root, int key);
Tree getMaxValueInTree(Tree parentNode, Tree root);


void main(int argc, char *argv[])
{
	FILE *fi = fopen(argv[1], "r");
	char cv;
	int key;


	Tree root = NULL;

	while (!feof(fi))
	{
		fscanf(fi, "%c", &cv);
		switch (cv)
		{
		case 'i':
			fscanf(fi, "%d", &key);
			root = insertNode(root, key);
			break;
		case 'd':
			fscanf(fi, "%d", &key);
			deleteNode(root, key);
			break;
		case 'f':
			fscanf(fi, "%d", &key);
			findNode(root, key);
			break;
		case 'p':
			fscanf(fi, "%c", &cv);
			if (cv == 'i')
			{
				printInorder(root);
			}
			printf("\n");
			break;
		}
	}
	deleteTree(root);
}

//Lab6
Tree insertNode(Tree root, int key)
{
	if (root == NULL)
	{
		root = malloc(sizeof(struct BinarySearchTree));
		root->value = key;
		root->left = NULL;
		root->right = NULL;
		printf("insert %d\n", key);
	}
	else if (root->value == key)
	{
		printf("Insertion Error : There is already %d in the tree.\n", key);
	}

	if (key < root->value) { root->left = insertNode(root->left, key); }
	else if (key > root->value) { root->right = insertNode(root->right, key);}
	
	return root;
}

void findNode(Tree root, int key)
{
	if (root == NULL) { printf("%d is not in the tree.\n", key); }

	if (key < root->value) { findNode(root->left, key); }
	else if (key > root->value) { findNode(root->right, key); }
	else { printf("%d is in the tree\n", key); }//if it has value in tree

}
void printInorder(Tree root)
{
	if (root->left != NULL) { printInorder(root->left); }
	printf("%d  ", root->value);
	if (root->right != NULL) { printInorder(root->right); }
}
void deleteTree(Tree root)
{
	if (root->left != NULL) { deleteTree(root->left); }
	free(root);//Q&A
	if (root->right != NULL) { deleteTree(root->right); }
}

//HW6
Tree deleteNode(Tree root, int key)
{
	Tree tmpCell = NULL;

	if (root == NULL)
	{
		printf("Deletion Error : %d is not in tree", key);
	}
	else if (key < root->value) { root->left = deleteNode(root->left, key); }
	else if (key > root->value) { root->right = deleteNode(root->right, key); }
	else if (root->left && root->right)
	{
		tmpCell = getMaxValueInTree(root->left, root);
		root->value = tmpCell->value;
		root->left = deleteNode(root->left, root->value);
	}
	else
	{
		tmpCell = root;
		if (root->left == NULL)
		{
			root = root->right;
		}
		else if (root->right == NULL)
		{
			root = root->left;
		}
		free(tmpCell);
	}
	return root;
}

Tree getMaxValueInTree(Tree parentNode, Tree root)
{
	if (parentNode == NULL)
	{
		return NULL;
	}
	else if (parentNode->right == NULL) 
	{
		 return parentNode
	}
	else
	{
		parentNode(parentNode->right, root);
	}
}