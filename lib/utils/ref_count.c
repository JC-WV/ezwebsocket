/**
 * \file      ref_count.c
 * \author    Clemens Kresser
 * \date      Mar 26, 2017
 * \copyright Copyright 2017-2021 Clemens Kresser MIT License
 * \brief     Simple reference counting implementation
 *
 */

#include <stdio.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include <ezwebsocket_log.h>
#if (__STDC_VERSION__ >= 201112L)
#include <stdatomic.h>
#endif

//! structure with data needed to store the references
struct ref_cnt_obj
{
  //! the number of reference holders
  volatile int cnt;
  //! the function that should be called to free the memory
  void (*pfnFree)(void*);
  //! mutex for the cnt variable
  pthread_mutex_t lock;
  //! the data itself
  unsigned char data[];
};

/**
 * \brief Allocates a buffer with reference counting
 *
 * \param size The wanted size
 * \param *pfnFree Function that frees elements inside the buffer if necessary the buffer is freed on it's own
 *                            or NULL if not needed
 *
 * \return Pointer to the allocated buffer
 */
void *refcnt_allocate(size_t size, void (*pfnFree)(void*))
{
  struct ref_cnt_obj *ref;

  ref = malloc(sizeof(struct ref_cnt_obj) + size);
  if(!ref)
  {
    ezwebsocket_log(EZLOG_ERROR, "malloc failed\n");
    return NULL;
  }
  ref->cnt = 1;
  ref->pfnFree = pfnFree;
#if (__STDC_VERSION__ < 201112L)
  pthread_mutex_init(&ref->lock, NULL);
#endif

  return ref->data;
}

/**
 * \brief Increments the reference count of the given object
 *
 * \param *ptr Pointer to the object
 */
void refcnt_ref(void *ptr)
{
  struct ref_cnt_obj *ref;

  ref = (struct ref_cnt_obj*)(((unsigned char *)ptr) - offsetof(struct ref_cnt_obj, data));
#if (__STDC_VERSION__ < 201112L)
  pthread_mutex_lock(&ref->lock);
  ref->cnt++;
  pthread_mutex_unlock(&ref->lock);
#else
  atomic_fetch_add((int *)&ref->cnt, 1);
#endif
}

/**
 * \brief Decrements the reference count of the given object and frees it if necessary
 *
 * \param *ptr Pointer to the object
 */
void refcnt_unref(void *ptr)
{
  struct ref_cnt_obj *ref;

  ref = (struct ref_cnt_obj*)(((unsigned char*)ptr) - offsetof(struct ref_cnt_obj, data));
#if (__STDC_VERSION__ < 201112L)
  pthread_mutex_lock(&ref->lock);
  if (ref->cnt >= 0)
    ref->cnt--;
  else
    ezwebsocket_log(EZLOG_ERROR, "too many unrefs\n");
  pthread_mutex_unlock(&ref->lock);
#else
  atomic_fetch_sub((int *)&ref->cnt, 1);
  if (ref->cnt < 0)
    ezwebsocket_log(EZLOG_ERROR, "too many unrefs\n");
#endif

  if(ref->cnt == 0)
  {
#if (__STDC_VERSION__ < 201112L)
    pthread_mutex_destroy(&ref->lock);
#endif
    if(ref->pfnFree)
      ref->pfnFree(ref->data);
    free(ref);
  }
}
