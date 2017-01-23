#ifndef __LINUX_NETFILTER_H
#define __LINUX_NETFILTER_H

#ifdef __KERNEL__
#include <linux/init.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/net.h>
#include <linux/if.h>
#include <linux/wait.h>
#include <linux/list.h>
#endif

#define NF_DROP 0
#define NF_ACCEPT 1
#define NF_STOLEN 2
#define NF_QUEUE 3
#define NF_REPEAT 4
#define NF_MAX_VERDICT NF_REPEAT

#define NFC_ALTERED 0x8000
#define NFC_UNKNOWN 0x4000

#ifdef __KERNEL__
#include <linux/config.h>
#ifdef CONFIG_NETFILTER

extern void netfilter_init(void);

#define NF_MAX_HOOKS 8

struct sk_buff;
struct net_device;

typedef unsigned int nf_hookfn(unsigned int hooknum,
			       struct sk_buff **skb,
			       const struct net_device *in,
			       const struct net_device *out,
			       int (*okfn)(struct sk_buff *));

struct nf_hook_ops
{
	struct list_head list;

	
	nf_hookfn *hook;
	int pf;
	int hooknum;
	
	int priority;
};

struct nf_sockopt_ops
{
	struct list_head list;

	int pf;

	
	int set_optmin;
	int set_optmax;
	int (*set)(struct sock *sk, int optval, void *user, unsigned int len);

	int get_optmin;
	int get_optmax;
	int (*get)(struct sock *sk, int optval, void *user, int *len);

	
	unsigned int use;
	struct task_struct *cleanup_task;
};

struct nf_info
{
	
	struct nf_hook_ops *elem;
	
	
	int pf;
	unsigned int hook;
	struct net_device *indev, *outdev;
	int (*okfn)(struct sk_buff *);
};
                                                                                
int nf_register_hook(struct nf_hook_ops *reg);
void nf_unregister_hook(struct nf_hook_ops *reg);

int nf_register_sockopt(struct nf_sockopt_ops *reg);
void nf_unregister_sockopt(struct nf_sockopt_ops *reg);

extern struct list_head nf_hooks[NPROTO][NF_MAX_HOOKS];



#ifdef CONFIG_NETFILTER_DEBUG
#define NF_HOOK(pf, hook, skb, indev, outdev, okfn)			\
 nf_hook_slow((pf), (hook), (skb), (indev), (outdev), (okfn), INT_MIN)
#define NF_HOOK_THRESH nf_hook_slow
#else
#define NF_HOOK(pf, hook, skb, indev, outdev, okfn)			\
(list_empty(&nf_hooks[(pf)][(hook)])					\
 ? (okfn)(skb)								\
 : nf_hook_slow((pf), (hook), (skb), (indev), (outdev), (okfn), INT_MIN))
#define NF_HOOK_THRESH(pf, hook, skb, indev, outdev, okfn, thresh)	\
(list_empty(&nf_hooks[(pf)][(hook)])					\
 ? (okfn)(skb)								\
 : nf_hook_slow((pf), (hook), (skb), (indev), (outdev), (okfn), (thresh)))
#endif

int nf_hook_slow(int pf, unsigned int hook, struct sk_buff *skb,
		 struct net_device *indev, struct net_device *outdev,
		 int (*okfn)(struct sk_buff *), int thresh);

int nf_setsockopt(struct sock *sk, int pf, int optval, char *opt, 
		  int len);
int nf_getsockopt(struct sock *sk, int pf, int optval, char *opt,
		  int *len);

typedef int (*nf_queue_outfn_t)(struct sk_buff *skb, 
                                struct nf_info *info, void *data);
extern int nf_register_queue_handler(int pf, 
                                     nf_queue_outfn_t outfn, void *data);
extern int nf_unregister_queue_handler(int pf);
extern void nf_reinject(struct sk_buff *skb,
			struct nf_info *info,
			unsigned int verdict);

extern void (*ip_ct_attach)(struct sk_buff *, struct nf_ct_info *);

#ifdef CONFIG_NETFILTER_DEBUG
extern void nf_dump_skb(int pf, struct sk_buff *skb);
#endif

extern void nf_invalidate_cache(int pf);

#else 
#define NF_HOOK(pf, hook, skb, indev, outdev, okfn) (okfn)(skb)
#endif 


#define SMAX(a,b) ((ssize_t)(a)>(ssize_t)(b) ? (ssize_t)(a) : (ssize_t)(b))
#define SMIN(a,b) ((ssize_t)(a)<(ssize_t)(b) ? (ssize_t)(a) : (ssize_t)(b))

#define UMAX(a,b) ((size_t)(a)>(size_t)(b) ? (size_t)(a) : (size_t)(b))
#define UMIN(a,b) ((size_t)(a)<(size_t)(b) ? (size_t)(a) : (size_t)(b))

#define SUMAX(a,b) ((size_t)(a)>(size_t)(b) ? (ssize_t)(a) : (ssize_t)(b))
#define SUMIN(a,b) ((size_t)(a)<(size_t)(b) ? (ssize_t)(a) : (ssize_t)(b))
#endif 

#endif 
