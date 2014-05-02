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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sync/waitgroup.h>
#include <runner/taskpool.h>
#include "xgb.h"

/* Backend poller wait kernel timeout msec */
#define DEF_ELOOPTIMEOUT 1
#define DEF_ELOOPIOMAX 100

struct xglobal xgb = {};

static void __shutdown_socks_task_hndl(struct xcpu *cpu) {
    struct xtask *ts, *nx_ts;
    struct list_head st_head = {};

    INIT_LIST_HEAD(&st_head);
    mutex_lock(&cpu->lock);
    efd_unsignal(&cpu->efd);
    list_splice(&cpu->shutdown_socks, &st_head);
    mutex_unlock(&cpu->lock);

    xtask_walk_safe(ts, nx_ts, &st_head) {
	list_del_init(&ts->link);
	ts->f(ts);
    }
}

static int cpu_task_hndl(eloop_t *el, ev_t *et) {
    return 0;
}

static inline int kcpud(void *args) {
    waitgroup_t *wg = (waitgroup_t *)args;
    int rc = 0;
    int cpu_no = xcpu_alloc();
    struct xcpu *cpu = xcpuget(cpu_no);

    mutex_init(&cpu->lock);
    INIT_LIST_HEAD(&cpu->shutdown_socks);

    /* Init eventloop and wakeup parent */
    BUG_ON(eloop_init(&cpu->el, XIO_MAX_SOCKS/XIO_MAX_CPUS,
		      DEF_ELOOPIOMAX, DEF_ELOOPTIMEOUT) != 0);
    BUG_ON(efd_init(&cpu->efd));
    cpu->efd_et.events = EPOLLIN|EPOLLERR;
    cpu->efd_et.fd = cpu->efd.r;
    cpu->efd_et.f = cpu_task_hndl;
    cpu->efd_et.data = cpu;
    BUG_ON(eloop_add(&cpu->el, &cpu->efd_et) != 0);

    /* Init done. wakeup parent thread */
    waitgroup_done(wg);

    while (!xgb.exiting) {
	eloop_once(&cpu->el);
	__shutdown_socks_task_hndl(cpu);
    }

    /* Release the poll descriptor when kcpud exit. */
    xcpu_free(cpu_no);
    eloop_destroy(&cpu->el);
    mutex_destroy(&cpu->lock);
    return rc;
}


struct xsock_protocol *l4proto_lookup(int pf, int type) {
    struct xsock_protocol *l4proto, *nx;

    xsock_protocol_walk_safe(l4proto, nx, &xgb.xsock_protocol_head) {
	if (pf == l4proto->pf && l4proto->type == type)
	    return l4proto;
    }
    return 0;
}

extern struct xsock_protocol xinp_listener_protocol;
extern struct xsock_protocol xinp_connector_protocol;
extern struct xsock_protocol xipc_listener_protocol;
extern struct xsock_protocol xipc_connector_protocol;
extern struct xsock_protocol xtcp_listener_protocol;
extern struct xsock_protocol xtcp_connector_protocol;
extern struct xsock_protocol xmul_listener_protocol[3];

void xsocket_module_init() {
    waitgroup_t wg;
    int xd;
    int cpu_no;
    int i;
    struct list_head *protocol_head = &xgb.xsock_protocol_head;

    BUG_ON(TP_TCP != XPF_TCP);
    BUG_ON(TP_IPC != XPF_IPC);
    BUG_ON(TP_INPROC != XPF_INPROC);


    xgb.exiting = false;
    mutex_init(&xgb.lock);

    for (xd = 0; xd < XIO_MAX_SOCKS; xd++)
	xgb.unused[xd] = xd;
    for (cpu_no = 0; cpu_no < XIO_MAX_CPUS; cpu_no++)
	xgb.cpu_unused[cpu_no] = cpu_no;

    xgb.cpu_cores = 1;
    taskpool_init(&xgb.tpool, xgb.cpu_cores);
    taskpool_start(&xgb.tpool);

    waitgroup_init(&wg);
    waitgroup_adds(&wg, xgb.cpu_cores);
    for (i = 0; i < xgb.cpu_cores; i++)
	taskpool_run(&xgb.tpool, kcpud, &wg);
    /* Waiting all poll's kcpud start properly */
    waitgroup_wait(&wg);
    waitgroup_destroy(&wg);
    
    /* The priority of xsock_protocol: inproc > ipc > tcp */
    INIT_LIST_HEAD(protocol_head);
    list_add_tail(&xinp_listener_protocol.link, protocol_head);
    list_add_tail(&xinp_connector_protocol.link, protocol_head);
    list_add_tail(&xipc_listener_protocol.link, protocol_head);
    list_add_tail(&xipc_connector_protocol.link, protocol_head);
    list_add_tail(&xtcp_listener_protocol.link, protocol_head);
    list_add_tail(&xtcp_connector_protocol.link, protocol_head);
    list_add_tail(&xmul_listener_protocol[0].link, protocol_head);
    list_add_tail(&xmul_listener_protocol[1].link, protocol_head);
    list_add_tail(&xmul_listener_protocol[2].link, protocol_head);
    list_add_tail(&xmul_listener_protocol[3].link, protocol_head);
}

void xsocket_module_exit() {
    DEBUG_OFF();
    xgb.exiting = true;
    taskpool_stop(&xgb.tpool);
    taskpool_destroy(&xgb.tpool);
    mutex_destroy(&xgb.lock);
}
