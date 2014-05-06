/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#ifndef _HPIO_SP_MODULE_
#define _HPIO_SP_MODULE_

#include <uuid/uuid.h>
#include <ds/list.h>
#include <sync/mutex.h>
#include <sync/atomic.h>
#include <sync/spin.h>
#include <sync/condition.h>
#include <runner/thread.h>
#include <xio/poll.h>
#include <socket/xmsg.h>

struct epbase;
struct epbase_vfptr {
    int sp_family;
    int sp_type;
    struct epbase *(*alloc) ();
    void (*destroy) (struct epbase *ep);
    void (*add) (struct epbase *ep, char *xmsg);
    void (*rm) (struct epbase *ep, char **xmsg);
    int (*setopt) (struct epbase *ep, int opt, void *optval, int optlen);
    int (*getopt) (struct epbase *ep, int opt, void *optval, int *optlen);
    struct list_head item;
};

struct epsk {
    struct epbase *owner;
    struct xpoll_event ent;
    int fd;
    uuid_t uuid;
    struct list_head item;
};

struct epsk *epsk_new();
void epsk_free(struct epsk *sk);

struct epbase {
    struct epbase_vfptr vfptr;
    atomic_t ref;
    int eid;
    mutex_t lock;
    condition_t cond;
    struct xpoll_event ent;
    struct {
	int wnd;
	int buf;
	int waiters;
	struct list_head head;
    } rcv, snd;
    struct list_head listeners;
    struct list_head connectors;
    struct list_head bad_socks;
};

#define walk_msg_safe(msg, nmsg, head)					\
    list_for_each_entry_safe(msg, nmsg, head, struct xmsg, item)

#define walk_epsk_safe(sk, nsk, head)				\
    list_for_each_entry_safe(sk, nsk, head, struct epsk, item)



#define XIO_MAX_ENDPOINTS 1024

struct sp_global {
    int exiting;
    spin_t lock;

    /* The global table of existing ep. The descriptor representing
     * the ep is the index to this table. This pointer is also used to
     * find out whether context is initialised. If it is null, context is
     * uninitialised.
     */
    struct epbase *endpoints[XIO_MAX_ENDPOINTS];

    /* Stack of unused ep descriptors.  */
    int unused[XIO_MAX_ENDPOINTS];

    /* Number of actual ep. */
    size_t nendpoints;

    struct xpoll_t *po;
    thread_t po_routine;
    struct list_head epbase_head;
};

extern struct sp_global sg;

#define walk_epbase_vfptr(ep, tmp, head)		\
    list_for_each_entry_safe(ep, tmp, head,		\
			     struct epbase_vfptr, item)

static inline
struct epbase_vfptr *epbase_vfptr_lookup(int sp_family, int sp_type) {
    struct epbase_vfptr *vfptr, *tmp;

    walk_epbase_vfptr(vfptr, tmp, &sg.epbase_head) {
	if (vfptr->sp_family == sp_family && vfptr->sp_type == sp_type)
	    return vfptr;
    }
    return 0;
}

int eid_alloc(int sp_family, int sp_type);
struct epbase *eid_get(int eid);
void eid_put(int eid);




#endif
