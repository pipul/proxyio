#ifndef _HPIO_PROXY_
#define _HPIO_PROXY_

#include "core.h"
#include "rio.h"

typedef int (*walkfn) (proxy_t *py, struct role *r, void *args);

#define list_for_each_role_safe(pos, n, head)				\
    list_for_each_entry_safe(pos, n, head, struct role, py_link)

#define proxy_lock(py) spin_lock(&py->lock)
#define proxy_unlock(py) spin_unlock(&py->lock)

static inline proxy_t *proxy_new() {
    proxy_t *py = (proxy_t *)mem_zalloc(sizeof(*py));
    return py;
}

static inline void proxy_init(proxy_t *py) {
    spin_init(&(py)->lock);
    ssmap_init(&(py)->roles);
    ssmap_init(&(py)->tw_roles);
    INIT_LIST_HEAD(&(py)->rcver_head);
    INIT_LIST_HEAD(&(py)->snder_head);
}

static inline void proxy_destroy(proxy_t *py) {
    struct role *r, *n;		

    spin_destroy(&(py)->lock);	
    list_for_each_role_safe(r, n, &(py)->rcver_head) {	
	r_destroy(r);					
	mem_free(r, sizeof(*r));			
    }							
    list_for_each_role_safe(r, n, &(py)->snder_head) {	
	r_destroy(r);					
	mem_free(r, sizeof(*r));			
    }							
}

static inline void proxy_add(proxy_t *py, struct role *r) {
    struct list_head *__head =
	IS_RCVER(r) ? &py->rcver_head : &py->snder_head;

    proxy_lock(py);							
    py->rsize++;							
    ssmap_insert(&py->roles, &r->sibling);				
    list_add(&r->py_link, __head);					
    proxy_unlock(py);						
}

static inline void proxy_del(proxy_t *py, struct role *r) {
    proxy_lock(py);				
    py->rsize--;				
    ssmap_delete(&py->roles, &r->sibling);	
    list_del(&r->py_link);			
    proxy_unlock(py);			
}

static inline void __proxy_walk(proxy_t *py, walkfn f, void *args,
			      struct list_head *head) {
    struct role *r = NULL, *nxt = NULL;	
    proxy_lock(py);				
    list_for_each_role_safe(r, nxt, head)	
	f(py, r, args);			
    proxy_unlock(py);			
}

#define proxy_walkrcver(py, f, args) __proxy_walk((py), (f), (args), (&py->rcver_head))
#define proxy_walksnder(py, f, args) __proxy_walk((py), (f), (args), (&py->snder_head))

static inline struct role *__proxy_find(proxy_t *py, uuid_t uuid, ssmap_t *map) {
    ssmap_node_t *node;
    struct role *r = NULL;
    if (!(node = ssmap_find(map, (char *)uuid, sizeof(uuid))))
	r = container_of(node, struct role, sibling);
    return r;
}

static inline struct role *proxy_find_at(proxy_t *py, uuid_t uuid) {
    struct role *r;
    proxy_lock(py);
    r = __proxy_find(py, uuid, &py->roles);
    proxy_unlock(py);
    return r;
}

static inline struct role *proxy_find_tw(proxy_t *py, uuid_t uuid) {
    struct role *r;
    proxy_lock(py);
    r = __proxy_find(py, uuid, &py->tw_roles);
    proxy_unlock(py);
    return r;
}

static inline struct role *proxy_loadbalance_dispatch(proxy_t *py) {
    struct role *dest = NULL;
    if (!list_empty(&py->snder_head)) {
	dest = list_first(&py->snder_head, struct role, py_link);
	list_del(&dest->py_link);
	list_add_tail(&dest->py_link, &py->snder_head);
    }
    return dest;
}


#endif