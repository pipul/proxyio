#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runner/taskpool.h"
#include "channel_base.h"

extern struct channel_global cn_global;

extern struct channel *cid_to_channel(int cd);
extern void free_channel(struct channel *cn);

static int channel_put(struct channel *cn) {
    int old;
    mutex_lock(&cn->lock);
    old = cn->proc.ref--;
    cn->fok = false;
    mutex_unlock(&cn->lock);
    return old;
}

static struct channel *find_listener(char *addr) {
    struct ssmap_node *node;
    struct channel *cn = NULL;

    cn_global_lock();
    if ((node = ssmap_find(&cn_global.inproc_listeners, addr, TP_SOCKADDRLEN)))
	cn = cont_of(node, struct channel, proc.listener_node);
    cn_global_unlock();
    return cn;
}

static int insert_listener(struct ssmap_node *node) {
    int rc = -1;

    errno = EADDRINUSE;
    cn_global_lock();
    if (!ssmap_find(&cn_global.inproc_listeners, node->key, node->keylen)) {
	rc = 0;
	ssmap_insert(&cn_global.inproc_listeners, node);
    }
    cn_global_unlock();
    return rc;
}


static void remove_listener(struct ssmap_node *node) {
    cn_global_lock();
    ssmap_delete(&cn_global.inproc_listeners, node);
    cn_global_unlock();
}


static void push_new_connector(struct channel *cn, struct channel *new) {
    mutex_lock(&cn->lock);
    list_add_tail(&new->proc.wait_item, &cn->proc.new_connectors);
    mutex_unlock(&cn->lock);
}

static struct channel *pop_new_connector(struct channel *cn) {
    struct channel *new = NULL;

    mutex_lock(&cn->lock);
    if (!list_empty(&cn->proc.new_connectors)) {
	new = list_first(&cn->proc.new_connectors, struct channel, proc.wait_item);
	list_del_init(&new->proc.wait_item);
    }
    mutex_unlock(&cn->lock);
    return new;
}

/******************************************************************************
 *  snd_head events trigger.
 ******************************************************************************/

static struct channel_msg *pop_snd(struct channel *cn) {
    struct channel_msg *msg = NULL;
    
    mutex_lock(&cn->lock);
    if (!list_empty(&cn->snd_head)) {
	msg = list_first(&cn->snd_head, struct channel_msg, item);
	list_del_init(&msg->item);

	/* Wakeup the blocking waiters */
	if (cn->snd_waiters > 0)
	    condition_broadcast(&cn->cond);
    }
    mutex_unlock(&cn->lock);
    return msg;
}

static void push_rcv(struct channel *cn, struct channel_msg *msg) {
    mutex_lock(&cn->lock);
    list_add_tail(&msg->item, &cn->rcv_head);

    /* Wakeup the blocking waiters. */
    if (cn->rcv_waiters > 0)
	condition_broadcast(&cn->cond);
    mutex_unlock(&cn->lock);
}

static int snd_head_push(struct list_head *head) {
    int rc = 0, can = false;
    struct channel_msg *msg;
    struct channel *cn = cont_of(head, struct channel, snd_head);
    struct channel *peer = cn->proc.peer_channel;

    // TODO: maybe the peer channel can't recv anymore after the check.
    mutex_lock(&peer->lock);
    if (can_recv(peer))
	can = true;
    mutex_unlock(&peer->lock);
    if (!can)
	return -1;
    if ((msg = pop_snd(cn)))
	push_rcv(peer, msg);
    return rc;
}

static int snd_head_pop(struct list_head *head) {
    int rc = 0;
    return rc;
}

static int snd_head_empty(struct list_head *head) {
    int rc = 0;
    return rc;
}

static int snd_head_nonempty(struct list_head *head) {
    int rc = 0;
    return rc;
}

static int snd_head_full(struct list_head *head) {
    int rc = 0;
    return rc;
}

static int snd_head_nonfull(struct list_head *head) {
    int rc = 0;
    return rc;
}

static struct head_vf snd_head_vf = {
    .push = snd_head_push,
    .pop = snd_head_pop,
    .empty = snd_head_empty,
    .nonempty = snd_head_nonempty,
    .full = snd_head_full,
    .nonfull = snd_head_nonfull,
};

/******************************************************************************
 *  rcv_head events trigger.
 ******************************************************************************/

static int rcv_head_push(struct list_head *head) {
    int rc = 0;
    return rc;
}

static int rcv_head_pop(struct list_head *head) {
    int rc = 0;
    struct channel *cn = cont_of(head, struct channel, rcv_head);
    mutex_lock(&cn->lock);
    if (cn->snd_waiters)
	condition_signal(&cn->cond);
    mutex_unlock(&cn->lock);
    return rc;
}

static int rcv_head_empty(struct list_head *head) {
    int rc = 0;
    return rc;
}

static int rcv_head_nonempty(struct list_head *head) {
    int rc = 0;
    return rc;
}

static int rcv_head_full(struct list_head *head) {
    int rc = 0;
    return rc;
}

static int rcv_head_nonfull(struct list_head *head) {
    int rc = 0;
    return rc;
}

static struct head_vf rcv_head_vf = {
    .push = rcv_head_push,
    .pop = rcv_head_pop,
    .empty = rcv_head_empty,
    .nonempty = rcv_head_nonempty,
    .full = rcv_head_full,
    .nonfull = rcv_head_nonfull,
};


static int inproc_accepter_init(int cd) {
    int rc = 0;
    struct channel *me = cid_to_channel(cd);
    struct channel *peer;
    struct channel *parent = cid_to_channel(me->parent);

    /* step1. Pop a new connector from parent's channel queue */
    if (!(peer = pop_new_connector(parent)))
	return -1;

    /* step2. Hold the peer's lock and make a connection. */
    mutex_lock(&peer->lock);

    /* Each channel endpoint has one ref to another endpoint */
    peer->proc.peer_channel = me;
    me->proc.peer_channel = peer;

    /* Send the ACK singal to the other end.
     * Here only has two possible state:
     * 1. if peer->proc.ref == 0. the peer haven't enter step2 state.
     * 2. if peer->proc.ref == 1. the peer is waiting and we should
     *    wakeup him when we done the connect work.
     */
    assert(peer->proc.ref == 0 || peer->proc.ref == 1);
    if (peer->proc.ref == 1)
	condition_signal(&peer->cond);
    me->proc.ref = peer->proc.ref = 2;
    mutex_unlock(&peer->lock);

    return rc;
}

static int inproc_listener_init(int cd) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);
    struct ssmap_node *node = &cn->proc.listener_node;

    node->key = cn->addr;
    node->keylen = TP_SOCKADDRLEN;
    INIT_LIST_HEAD(&cn->proc.new_connectors);
    if ((rc = insert_listener(node)) < 0)
	return rc;
    return rc;
}


static int inproc_listener_destroy(int cd) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);
    struct channel *new;

    /* Avoiding the new connectors */
    remove_listener(&cn->proc.listener_node);

    while ((new = pop_new_connector(cn))) {
	mutex_lock(&new->lock);
	/* Sending a ECONNREFUSED signel to the other peer. */
	if (new->proc.ref-- == 1)
	    condition_signal(&new->cond);
	mutex_unlock(&new->lock);
    }

    /* Destroy the channel and free channel id. */
    free_channel(cn);
    return rc;
}


static int inproc_connector_init(int cd) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);
    struct channel *listener = find_listener(cn->peer);

    errno = ENOENT;
    if (!listener)
	return -1;
    cn->proc.ref = 0;
    cn->proc.peer_channel = NULL;

    /* step1. Push the new connector into listener's new_connectors
       queue */
    push_new_connector(listener, cn);

    /* step2. Hold lock and waiting for the connection established
     * if need. here only has two possible state too:
     * if cn->proc.ref == 0. we incr the ref indicate that i'm waiting.
     */
    mutex_lock(&cn->lock);
    if (cn->proc.ref == 0) {
	cn->proc.ref++;
	condition_wait(&cn->cond, &cn->lock);
    }

    /* step3. Check the connection status.
     * Maybe the other peer close the connection before the ESTABLISHED
     * if cn->proc.ref == 0. the peer was closed.
     * if cn->proc.ref == 2. the connect was established.
     */
    if (cn->proc.ref == -1)
	assert(cn->proc.ref == -1);
    else if (cn->proc.ref == 0)
	assert(cn->proc.ref == 0);
    else if (cn->proc.ref == 2)
	assert(cn->proc.ref == 2);
    if (cn->proc.ref == 0 || cn->proc.ref == -1) {
    	errno = ECONNREFUSED;
	rc = -1;
    }
    mutex_unlock(&cn->lock);
    return rc;
}

static int inproc_connector_destroy(int cd) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);    
    struct channel *peer = cn->proc.peer_channel;

    /* Destroy the channel and free channel id if i hold the last ref. */
    if (channel_put(peer) == 1)
	free_channel(peer);
    if (channel_put(cn) == 1)
	free_channel(cn);
    return rc;
}


static int inproc_channel_init(int cd) {
    struct channel *cn = cid_to_channel(cd);

    switch (cn->ty) {
    case CHANNEL_ACCEPTER:
	cn->rcv_notify = rcv_head_vf;
	cn->snd_notify = snd_head_vf;
	return inproc_accepter_init(cd);
    case CHANNEL_CONNECTOR:
	cn->rcv_notify = rcv_head_vf;
	cn->snd_notify = snd_head_vf;
	return inproc_connector_init(cd);
    case CHANNEL_LISTENER:
	return inproc_listener_init(cd);
    }
    return -EINVAL;
}

static void inproc_channel_destroy(int cd) {
    struct channel *cn = cid_to_channel(cd);

    switch (cn->ty) {
    case CHANNEL_ACCEPTER:
    case CHANNEL_CONNECTOR:
	inproc_connector_destroy(cd);
	break;
    case CHANNEL_LISTENER:
	inproc_listener_destroy(cd);
	break;
    default:
	assert(0);
    }
}

static int inproc_channel_setopt(int cd, int opt, void *val, int valsz) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    mutex_unlock(&cn->lock);
    return rc;
}

static int inproc_channel_getopt(int cd, int opt, void *val, int valsz) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);

    mutex_lock(&cn->lock);
    mutex_unlock(&cn->lock);
    return rc;
}

static struct channel_msg *pop_rcv(struct channel *cn) {
    struct channel_msg *msg = NULL;

    mutex_lock(&cn->lock);
    while (list_empty(&cn->rcv_head) && !cn->fasync) {
	cn->rcv_waiters++;
	condition_wait(&cn->cond, &cn->lock);
	cn->rcv_waiters--;
    }
    if (!list_empty(&cn->rcv_head)) {
	msg = list_first(&cn->rcv_head, struct channel_msg, item);
	list_del_init(&msg->item);
    }
    mutex_unlock(&cn->lock);
    if (msg && cn->rcv_notify.pop)
	cn->rcv_notify.pop(&cn->rcv_head);
    return msg;
}


static int inproc_channel_recv(int cd, char **payload) {
    int rc = 0;
    struct channel_msg *msg;
    struct channel *cn = cid_to_channel(cd);

    if (!cn->fok) {
	/* Only i hold the channel, the other peer shutdown. */
	errno = EPIPE;
	rc = -1;
    } else if (!(msg = pop_rcv(cn))) {
	/* Conditon race here when the peer channel shutdown
	   after above checking. it's ok. */
	errno = EAGAIN;
	rc = -1;
    } else
	*payload = msg->hdr.payload;
    return rc;
}


static int push_snd(struct channel *cn, struct channel_msg *msg) {
    int rc = -1;

    mutex_lock(&cn->lock);
    while (!can_send(cn) && !cn->fasync) {
	cn->snd_waiters++;
	condition_wait(&cn->cond, &cn->lock);
	cn->snd_waiters--;
    }
    if (can_send(cn)) {
	rc = 0;
	list_add_tail(&msg->item, &cn->snd_head);
    }
    mutex_unlock(&cn->lock);
    if (rc == 0 && cn->snd_notify.push)
	cn->snd_notify.push(&cn->snd_head);
    return rc;
}

static int inproc_channel_send(int cd, char *payload) {
    int rc = 0;
    struct channel *cn = cid_to_channel(cd);
    struct channel_msg *msg;

    msg = cont_of(payload, struct channel_msg, hdr.payload);
    /* Only i hold the channel, the other peer shutdown. */
    if (!cn->fok) {
	errno = EPIPE;
	return -1;
    }
    /* Conditon race here when the peer channel shutdown
       after above checking. it's ok. */
    rc = push_snd(cn, msg);
    return rc;
}

static struct channel_vf inproc_channel_vf = {
    .pf = PF_INPROC,
    .init = inproc_channel_init,
    .destroy = inproc_channel_destroy,
    .recv = inproc_channel_recv,
    .send = inproc_channel_send,
    .setopt = inproc_channel_setopt,
    .getopt = inproc_channel_getopt,
};

struct channel_vf *inproc_channel_vfptr = &inproc_channel_vf;
