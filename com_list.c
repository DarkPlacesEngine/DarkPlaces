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

// com_list.c - generic doubly linked list interface, adapted from Linux list.h

#include "qtypes.h"
#include "com_list.h"

/*
 * Creates a new linked list. Initializes the head to point to itself.
 * If it's a list header, the result is an empty list.
 */
void List_Create(llist_t *list)
{
	list->next = list->prev = NULL;
}

/*
 * Insert a node between two known nodes.
 * 
 * Only use when prev and next are known.
 */
static void __List_Add(llist_t *node, llist_t *prev, llist_t *next)
{
	next->prev = node;
	node->next = next;
	node->prev = prev;
	prev->next = node;
}

/*
 * Insert a node immediately after head.
 */
void List_Add(llist_t *node, llist_t *head)
{
	__List_Add(node, head, head->next);
}

/*
 * Insert a node immediately before head.
 */
void List_Add_Tail(llist_t *node, llist_t *head)
{
	__List_Add(node, head->prev, head);
}

/*
 * Bridge prev and next together, when removing the parent of them.
 */
static inline void __List_Delete(llist_t *prev, llist_t *next)
{
	next->prev = prev;
	prev->next = next;
}

/*
 * Redundant?
 */
static inline void __List_Delete_Node(llist_t *node)
{
	__List_Delete(node->prev, node->next);
}

/*
 * Removes a node from its list. Sets its pointers to NULL.
 */
void List_Delete(llist_t *node)
{
	__List_Delete_Node(node);
	node->next = node->prev = NULL;
}

/*
 * Removes a node from its list. Reinitialize it.
 */
void List_Delete_Init(llist_t *node)
{
	__List_Delete_Node(node);
	node->next = node->prev = node;
}

/*
 * Replace old with new. Old is overwritten if empty.
 */
void List_Replace(llist_t *old, llist_t *_new)
{
	_new->next = old->next;
	_new->next->prev = _new;
	_new->prev = old->prev;
	_new->prev->next = _new;
	old->next = old->prev = old;
}

/*
 * Replace old with new. Initialize old.
 * Old is overwritten if empty.
 */
void List_Replace_Init(llist_t *old, llist_t *_new)
{
	List_Replace(old, _new);
	List_Create(old);
}

/*
 * Swap node1 with node2 in place.
 */
void List_Swap(llist_t *node1, llist_t *node2)
{
	llist_t *pos = node2->prev;
	List_Delete_Init(node2);
	List_Replace(node1, node2);
	if(pos == node1)
		pos = node2;
	List_Add(node1, pos);
}

/*
 * Delete list from its... list, then insert after head.
 */
void List_Move(llist_t *list, llist_t *head)
{
	__List_Delete_Node(list);
	List_Add(list, head);
}

/*
 * Delete list from its... list, then insert before head.
 */
void List_Move_Tail(llist_t *list, llist_t *head)
{
	__List_Delete_Node(list);
	List_Add_Tail(list, head);
}

/*
 * Move the first node of a range of nodes immediately after head.
 * All three parameters must belong to the same list.
 */

void List_Bulk_Move_Tail(llist_t *head, llist_t *first, llist_t *last)
{
	first->prev->next = last->next;
	last->next->prev = first->prev;

	head->prev->next = first;
	first->prev = head->prev;

	last->next = head;
	head->prev = last;
}

/*
 * Shift the head to the right (like rotating a wheel counterclockwise).
 * The node immediately to the right becomes the new head.
 */
void List_Rotate_Left(llist_t *head)
{
	llist_t *first;

	if (!List_Is_Empty(head))
	{
		first = head->next;
		List_Move_Tail(first, head);
	}
}

/*
 * Make list the new head.
 */
void List_Rotate_To_Front(llist_t *list, llist_t *head)
{
	List_Move_Tail(head, list);
}

/*
 * Concatenate two lists. The head of list will be discarded.
 */
static inline void __List_Splice(const llist_t *list, llist_t *prev, llist_t *next)
{
	llist_t *first = list->next;
	llist_t *last = list->prev;

	first->prev = prev;
	prev->next = first;

	last->next = next;
	next->prev = last;
}

/*
 * Concatenate two lists. The first node of list will be inserted after head.
 */
void List_Splice(const llist_t *list, llist_t *head)
{
	if(!List_Is_Empty(list))
		__List_Splice(list, head, head->next);
}

/*
 * Concatenate two lists. The tail of list will be inserted before head.
 */
void List_Splice_Tail(const llist_t *list, llist_t *head)
{
	if (!List_Is_Empty(list))
		__List_Splice(list, head->prev, head);
}

qbool List_Is_First(llist_t *list, llist_t *start)
{
	return list->prev == start;
}

qbool List_Is_Last(llist_t *list, llist_t *start)
{
	return list->next == start;
}

qbool List_Is_Empty(const llist_t *list)
{
	return list->next == list;
}
