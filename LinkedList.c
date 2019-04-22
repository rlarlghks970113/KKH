#pragma warning (disable:4996) //scanf_s 에러무시
#include <stdio.h>

typedef struct Node* Node;
typedef struct Node* List;
typedef struct Node* Position;
struct Node {
	int element;
	Node nextNode;
};

List MakeList(List L);
void DeleteList(List L);
Position Find(List L, int key);
void Insert(List L, int key, Position P);
void Delete(List L, int key);

int IsEmpty(List L);
int IsLast(List L, Position P);
Position FindPrevious(List L, int key);
void PrintList(List L);



void main(int argc, char *argv[])
{
	char input = "";
	int n, position;
	List L = NULL;
	L = MakeList(L);

	while (input != 'q') {
		if (!IsEmpty(L)) PrintList(L);
		scanf("%c", &input);
		switch (input)
		{
		case 'p'://링크드리스트 프린트
			if(!IsEmpty(L)) PrintList(L);
			break;
		case 'i'://삽입
			
			scanf("%d %d", &n, &position);
			Node pos = Find(L, position);
			if(pos != NULL) Insert(L, n, pos);
			break;
		case 'f':
			scanf("%d", &n);
			if (Find(L, n) == NULL) printf("%d is not in List!\n", n);
			else printf("%d is in List!\n", n);
			break;
		case 'd':
			scanf("%d", &n);
			Delete(L, n);
			break;
		default:
			break;
		}
		getchar();
	}
	DeleteList(L);
}

List MakeList(List L)
{
	L = (List)malloc(sizeof(struct Node));
	L->element = 0;
	L->nextNode = NULL;

	return L;
}

void DeleteList(List L)
{
	Node tempNode = L;
	Node nextNode = L->nextNode;
	while (nextNode != NULL)
	{
		free(tempNode);
		tempNode = nextNode;
		nextNode = tempNode->nextNode;
	}
	free(tempNode);
}
Node Find(List L, int key)
{
	if (L == NULL) return NULL; //값이 없을때
	else if (L->element == key) return L;
	else return Find(L->nextNode, key);
}
void Insert(List L, int key, Position P)
{
	Node newNode = malloc(sizeof(struct Node));

	newNode->element = key;
	newNode->nextNode = P->nextNode;

	P->nextNode = newNode;
}
void Delete(List L, int key)
{
	Position previousNode = FindPrevious(L, key);
	Position tempNode = previousNode->nextNode;

	previousNode->nextNode = tempNode->nextNode;
	free(tempNode);
}
int IsEmpty(List L)
{
	if (L->nextNode == NULL) return 1;
	else return 0;
	
}
int IsLast(List L, Position P)
{
	if (P->nextNode == NULL) return 1;
	else return 0;
}
Position FindPrevious(List L, int key)
{
	Position P = L->nextNode;
	while (P->nextNode != NULL && P->nextNode->element != key)
	{
		P = P->nextNode;
	}
	return P;
}
void PrintList(List L)
{
	Position P = L->nextNode;
	while (P != NULL)
	{
		printf("%d  ", P->element);
		P = P->nextNode;
	}
	printf("\n");
}