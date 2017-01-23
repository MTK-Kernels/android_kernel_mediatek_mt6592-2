#ifndef __VKI_XEN_EVTCHN_H
#define __VKI_XEN_EVTCHN_H

#define VKI_XEN_EVTCHNOP_bind_interdomain 0
#define VKI_XEN_EVTCHNOP_bind_virq        1
#define VKI_XEN_EVTCHNOP_bind_pirq        2
#define VKI_XEN_EVTCHNOP_close            3
#define VKI_XEN_EVTCHNOP_send             4
#define VKI_XEN_EVTCHNOP_status           5
#define VKI_XEN_EVTCHNOP_alloc_unbound    6
#define VKI_XEN_EVTCHNOP_bind_ipi         7
#define VKI_XEN_EVTCHNOP_bind_vcpu        8
#define VKI_XEN_EVTCHNOP_unmask           9
#define VKI_XEN_EVTCHNOP_reset           10

typedef vki_uint32_t vki_xen_evtchn_port_t;
DEFINE_VKI_XEN_GUEST_HANDLE(vki_xen_evtchn_port_t);

struct vki_xen_evtchn_alloc_unbound {
    
    vki_xen_domid_t dom, remote_dom;
    
    vki_xen_evtchn_port_t port;
};

struct vki_xen_evtchn_op {
    vki_uint32_t cmd; 
    union {
        struct vki_xen_evtchn_alloc_unbound    alloc_unbound;
        
        
        
        
        
        
        
        
        
    } u;
};

#endif 

