/*
 * Copyright (c) 2015 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

typedef struct _Inlist Inlist;

struct _Inlist
{
   Inlist *next;
   Inlist *prev;
   Inlist *last;
};

#define INLIST Inlist __in_list
#define INLIST_GET(Inlist)         (& ((Inlist)->__in_list))
#define INLIST_CONTAINER_GET(ptr,                          \
                                  type) ((type *)((char *)ptr - \
                                                  offsetof(type, __in_list)))

#define SAFETY_ON_NULL_RETURN_VAL(in, out) \
	if(in == NULL) \
		return out;


static inline Inlist *
inlist_append(Inlist *list, Inlist *new_l)
{
   Inlist *l;

   SAFETY_ON_NULL_RETURN_VAL(new_l, list);

   new_l->next = NULL;
   if (!list)
     {
        new_l->prev = NULL;
        new_l->last = new_l;
        return new_l;
     }

   if (list->last)
     l = list->last;
   else
     for (l = list; (l) && (l->next); l = l->next)
       ;

   l->next = new_l;
   new_l->prev = l;
   list->last = new_l;
   return list;
}

static inline Inlist *
inlist_prepend(Inlist *list, Inlist *new_l)
{
   SAFETY_ON_NULL_RETURN_VAL(new_l, list);

   new_l->prev = NULL;
   if (!list)
     {
        new_l->next = NULL;
        new_l->last = new_l;
        return new_l;
     }

   new_l->next = list;
   list->prev = new_l;
   new_l->last = list->last;
   list->last = NULL;
   return new_l;
}

static inline Inlist *
inlist_remove(Inlist *list, Inlist *item)
{
   Inlist *return_l;

   /* checkme */
   SAFETY_ON_NULL_RETURN_VAL(list, NULL);
   SAFETY_ON_NULL_RETURN_VAL(item, list);
   if ((item != list) && (!item->prev) && (!item->next))
     {
        //FIXME LOG_ERR("safety check failed: item %p does not appear to be part of an inlist!", item);
        return list;
     }

   if (item->next)
     item->next->prev = item->prev;

   if (item->prev)
     {
        item->prev->next = item->next;
        return_l = list;
     }
   else
     {
        return_l = item->next;
        if (return_l)
          return_l->last = list->last;
     }

   if (item == list->last)
     list->last = item->prev;

   item->next = NULL;
   item->prev = NULL;
   return return_l;
}

static inline unsigned int
inlist_count(const Inlist *list)
{
   const Inlist *l;
   unsigned int i = 0;

   for (l = list; l; l = l->next)
     i++;

   return i;
}

#define _INLIST_OFFSET(ref)         ((char *)&(ref)->__in_list - (char *)(ref))

#if !defined(__cplusplus)
#define _INLIST_CONTAINER(ref, ptr) (void *)((char *)(ptr) - \
                                                  _INLIST_OFFSET(ref))
#else
/*
 * In C++ we can't assign a "type*" pointer to void* so we rely on GCC's typeof
 * operator.
 */
# define _INLIST_CONTAINER(ref, ptr) (__typeof__(ref))((char *)(ptr) - \
							    _INLIST_OFFSET(ref))
#endif

#define INLIST_FOREACH(list, it)                                     \
  for (it = NULL, it = (list ? _INLIST_CONTAINER(it, list) : NULL); it; \
       it = (INLIST_GET(it)->next ? _INLIST_CONTAINER(it, INLIST_GET(it)->next) : NULL))

#define INLIST_FOREACH_SAFE(list, list2, it) \
   for (it = NULL, it = (list ? _INLIST_CONTAINER(it, list) : NULL), list2 = it ? INLIST_GET(it)->next : NULL; \
        it; \
        it = NULL, it = list2 ? _INLIST_CONTAINER(it, list2) : NULL, list2 = list2 ? list2->next : NULL)

#define INLIST_REVERSE_FOREACH(list, it)                                \
  for (it = NULL, it = (list ? _INLIST_CONTAINER(it, list->last) : NULL); \
       it; it = (INLIST_GET(it)->prev ? _INLIST_CONTAINER(it, INLIST_GET(it)->prev) : NULL))

#define INLIST_REVERSE_FOREACH_FROM(list, it)                                \
  for (it = NULL, it = (list ? _INLIST_CONTAINER(it, list) : NULL); \
       it; it = (INLIST_GET(it)->prev ? _INLIST_CONTAINER(it, INLIST_GET(it)->prev) : NULL))

#define INLIST_FREE(list, it)				\
  for (it = (__typeof__(it)) list; list; it = (__typeof__(it)) list)
