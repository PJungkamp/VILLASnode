/** A generic linked list
 *
 * Linked lists a used for several data structures in the code.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <string.h>

#include "utils.h"
#include "list.h"

void list_init(struct list *l, dtor_cb_t dtor)
{
	pthread_mutex_init(&l->lock, NULL);

	l->destructor = dtor;
	l->length = 0;
	l->head = NULL;
	l->tail = NULL;
}

void list_destroy(struct list *l)
{
	pthread_mutex_lock(&l->lock);

	struct list_elm *elm = l->head;
	while (elm) {
		struct list_elm *tmp = elm;
		elm = elm->next;

		if (l->destructor)
			l->destructor(tmp->ptr);

		free(tmp);
	}

	pthread_mutex_unlock(&l->lock);
	pthread_mutex_destroy(&l->lock);
}

void list_push(struct list *l, void *p)
{
	struct list_elm *e = alloc(sizeof(struct list_elm));

	pthread_mutex_lock(&l->lock);

	e->ptr = p;
	e->prev = l->tail;
	e->next = NULL;

	if (l->tail)
		l->tail->next = e;
	if (l->head)
		l->head->prev = e;
	else
		l->head = e;

	l->tail = e;
	l->length++;

	pthread_mutex_unlock(&l->lock);
}

void list_insert(struct list *l, int prio, void *p)
{
	struct list_elm *d;
	struct list_elm *e = alloc(sizeof(struct list_elm));

	e->priority = prio;
	e->ptr = p;

	pthread_mutex_lock(&l->lock);

	/* Search for first entry with higher priority */
	for (d = l->head; d && d->priority < prio; d = d->next);

	/* Insert new element in front of d */
	e->next = d;

	if (d) { /* Between or Head */
		e->prev = d->prev;

		if (d == l->head) /* Head */
			l->head = e;
		else /* Between */
			d->prev = e;
	}
	else { /* Tail or Head */
		e->prev = l->tail;

		if (l->length == 0) /* List was empty */
			l->head = e;
		else
			l->tail->next = e;

		l->tail = e;
	}

	l->length++;

	pthread_mutex_unlock(&l->lock);
}

void * list_lookup(const struct list *l, const char *name)
{
	FOREACH(l, it) {
		if (!strcmp(*(char **) it->ptr, name))
			return it->ptr;
	}

	return NULL;
}

void * list_search(const struct list *l, cmp_cb_t cmp, void *ctx)
{
	FOREACH(l, it) {
		if (!cmp(it->ptr, ctx))
			return it->ptr;
	}

	return NULL;
}
