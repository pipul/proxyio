#ifndef _HPIO_PROXY_
#define _HPIO_PROXY_

#include <uuid/uuid.h>
#include <base.h>
#include <x/xsock.h>
#include <ds/map.h>
#include <ds/list.h>
#include <runner/thread.h>
#include <sync/mutex.h>
#include "ep_url.h"
#include "ep_fd.h"

extern const char *py_str[2];


struct pxy {
    u32 fstopped:1;
    mutex_t mtx;
    thread_t backend;
    struct xpoll_t *po;
    struct rtb tb;
    struct list_head listener;
    struct list_head unknown;
};


void pxy_init(struct pxy *y);
void pxy_destroy(struct pxy *y);
int pxy_listen(struct pxy *y, const char *url);
int pxy_connect(struct pxy *y, const char *url);
int pxy_onceloop(struct pxy *y);
int pxy_startloop(struct pxy *y);
void pxy_stoploop(struct pxy *y);


struct ep {
    int type;
    struct pxy y;
};




#endif