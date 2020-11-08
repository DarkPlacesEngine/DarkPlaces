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

// com_list.h - generic doubly linked list interface, adapted from Linux list.h

#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include "qtypes.h"
#include "qdefs.h"

typedef struct llist_s
{
	struct llist_s *prev;
	struct llist_s *next;
} llist_t;

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct llist_s name = LIST_HEAD_INIT(name)

/*
 * Get the struct for this entry
 */
#define List_Entry(ptr, type, member) ContainerOf(ptr, type, member)

/*
 * Get the first element from a list
 * The list is expected to not be empty
 */
#define List_First_Entry(ptr, type, member) List_Entry((ptr)->next, type, member)

/*
 * Get the last element from the list
 * The list is expected to not be empty
 */
#define List_Last_Entry(ptr, type, member) List_Entry((ptr)->prev, type, member)

/*
 * Get the first element from the list, but return NULL if it's empty
 */
#define List_First_Entry_Or_Null(ptr, type, member) ({ \
	struct llist_s *head__ = (ptr); \
	struct llist_s *pos__ = head__->next; \
	pos__ != head__ ? List_Entry(pos__, type, member) : NULL; \
})

/*
 * Get the next element in the list
 */
#define List_Next_Entry(pos, member) \
	List_Entry((pos)->member.next, Q_typeof(*(pos)), member)

/*
 * Get the prev element in the list
 */
#define List_Prev_Entry(pos, member) \
	List_Entry((pos)->member.prev, Q_typeof(*(pos)), member)

/*
 * Iterate over a list
 */
#define List_For_Each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

/*
 * Continue iteration over a list, after the current position
 */
#define List_For_Each_Continue(pos, head) \
	for (pos = pos->next; pos != (head); pos = pos->next)

/*
 * Iterate over a list backwards
 */
#define List_For_Each_Prev(pos, head) \
	for (pos = (head)->prev; pos != (head); pos = pos->prev)

/*
 * Iterate over a list, safe against removal of list entry
 */
#define List_For_Each_Safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
	     pos = n, n = pos->next)
/*
 * Iterate over a list backwards, safe against removal of list entry
 */
#define List_For_Each_Prev_Safe(pos, n, head) \
	for (pos = (head)->prev, n = pos->prev; \
	     pos != (head); \
	     pos = n, n = pos->prev)
/*
 * Test if the entry points to the head of the list
 */
#define List_Entry_Is_Head(pos, head, member) \
	(&pos->member == (head))

/*
 * Iterate over a list of a given type
 */
#define List_For_Each_Entry(pos, head, member) \
	for (pos = List_Last_Entry(head, Q_typeof(*pos), member); \
	     !List_Entry_Is_Head(pos, head, member); \
	     pos = List_Prev_Entry(pos, member))

/*
 * Iterate over a list of a given type backwards
 */
#define List_For_Each_Prev_Entry(pos, head, member) \
	for (pos = List_Last_Entry(head, Q_typeof(*pos), member); \
	     !List_Entry_Is_Head(pos, head, member); \
	     pos = List_Prev_Entry(pos, member))

/*
 * Prepares a pos entry for use as a start point in List_For_Each_Entry_Continue()
 */
#define List_Prepare_Entry(pos, head, member) \
	((pos) ? : List_Entry(head, Q_typeof(*pos), member))

/*
 * Continue iteration over a list of a given type, after the current position
 */
#define List_For_Each_Entry_Continue(pos, head, member) \
	for (pos = List_Next_Entry(pos, member); \
	     !List_Entry_Is_Head(pos, head, member); \
	     pos = List_Next_Entry(pos, member))

/*
 * Continue iteration over a list of a given type backwards, after the current position
 */
#define List_For_Each_Prev_Entry_Continue(pos, head, member) \
	for (pos = List_Prev_Entry(pos, member); \
	     !List_Entry_Is_Head(pos, head, member); \
	     pos = List_Prev_Entry(pos, member))

/*
 * Continue iteration over a list of a given type, from the current position
 */
#define List_For_Each_Entry_From(pos, head, member) \
	for (; !List_Entry_Is_Head(pos, head, member); \
	     pos = List_Next_Entry(pos, member))

/*
 * Continue iteration over a list of a given type backwards, from the current position
 */
#define List_For_Each_Prev_Entry_From(pos, head, member) \
	for (; !List_Entry_Is_Head(pos, head, member); \
	     pos = List_Prev_Entry(pos, member))

/*
 * Iterate over a list of a given type, safe against removal of list entry
 */
#define List_For_Each_Entry_Safe(pos, n, head, member) \
	for (pos = List_First_Entry(head, Q_typeof(*pos), member), \
	     n = List_Next_Entry(pos, member); \
	     !List_Entry_Is_Head(pos, head, member); \
	     pos = n, n = List_Next_Entry(n, member))
/*
 * Continue iteration over a list of a given type, after the current position, safe against removal of list entry
 */
#define List_For_Each_Entry_Safe_Continue(pos, n, head, member) \
	for (pos = List_Next_Entry(pos, member), \
	     n = List_Next_Entry(pos, member); \
	     !List_Entry_Is_Head(pos, head, member); \
	     pos = n, n = List_Next_Entry(n, member))

/*
 * Continue iteration over a list of a given type, from the current position, safe against removal of list entry
 */
#define List_For_Each_Entry_Safe_From(pos, n, head, member) \
	for (n = List_Next_Entry(pos, member); \
	     !List_Entry_Is_Head(pos, head, member); \
	     pos = n, n = List_Next_Entry(n, member))


/*
 * Iterate over a list of a given type backwards, safe against removal of list entry
 */
#define List_For_Each_Prev_Entry_Safe(pos, n, head, member) \
	for (pos = List_Last_Entry(head, Q_typeof(*pos), member), \
	     n = List_Prev_Entry(pos, member); \
	     !List_Entry_Is_Head(pos, head, member); \
	     pos = n, n = List_Prev_Entry(n, member))

/*
 * Reset a stale List_For_Each_Entry_Safe loop
 */
#define List_Safe_Reset_Next(pos, n, member) \
		n = List_Next_Entry(pos, member)

void List_Create(llist_t *list);
void List_Add(llist_t *node, llist_t *start);
void List_Add_Tail(llist_t *node, llist_t *start);
void List_Delete(llist_t *node);
void List_Delete_Init(llist_t *node);
void List_Replace(llist_t *old, llist_t *_new);
void List_Replace_Init(llist_t *old, llist_t *_new);
void List_Swap(llist_t *node1, llist_t *node2);
void List_Move(llist_t *list, llist_t *start);
void List_Move_Tail(llist_t *list, llist_t *start);
void List_Bulk_Move_Tail(llist_t *start, llist_t *first, llist_t *last);
void List_Rotate_Left(llist_t *head);
void List_Rotate_To_Front(llist_t *list, llist_t *head);
void List_Splice(const llist_t *list, llist_t *head);
void List_Splice_Tail(const llist_t *list, llist_t *head);
qbool List_Is_First(llist_t *list, llist_t *start);
qbool List_Is_Last(llist_t *list, llist_t *start);
qbool List_Is_Empty(const llist_t *list);

#endif
