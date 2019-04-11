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
			fscnaf(fi, "%d", &key);
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
	}
	if (key < root->value) { root->left = insertNode(root->left, key); }
	else if (key > root->value) { root->right = insertNode(root->right, key);}
	else { printf("Insertion Error : There is already %d in the tree.\n", key); }//if it has value in tree

	
	return root;
}

void findNode(Tree root, int key)
{
	if (root == NULL) { printf("%d is not in the tree.\n"); }

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