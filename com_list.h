/*
Copyright (C) 2020 David "Cloudwalk" Knapp

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// com_list.c - generic doubly linked list interface, inspired by Linux list.h

#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include "qtypes.h"

typedef struct llist_s
{
	struct llist_s *prev;
	struct llist_s *next;
} llist_t;

#define List_Head_Reset(name) { &(name), &(name) }

#define List_Container(ptr, type, member) ContainerOf(ptr, type, member)

#define List_ForEach(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#define List_ForEach_Prev(pos, head) \
	for (pos = (head)->prev; pos != (head); pos = pos->prev)

void List_Add(llist_t *node, llist_t *start);
void List_Add_Tail(llist_t *node, llist_t *start);
void List_Delete(llist_t *node);
void List_Delete_Init(llist_t *node);
void List_Replace(llist_t *old, llist_t *_new);
void List_Swap(llist_t *node1, llist_t *node2);
void List_Move(llist_t *list, llist_t *start);
void List_Move_Tail(llist_t *list, llist_t *start);
void List_Bulk_Move_Tail(llist_t *start, llist_t *first, llist_t *last);
void List_Rotate_Left(llist_t *head);
void List_Rotate_To_Front(llist_t *list, llist_t *head);
void List_Splice(const llist_t *list, llist_t *head);
void List_Splice_Tail(const llist_t *list, llist_t *head);
qbool List_IsFirst(llist_t *list, llist_t *start);
qbool List_IsLast(llist_t *list, llist_t *start);
qbool List_IsEmpty(const llist_t *list);

#endif
