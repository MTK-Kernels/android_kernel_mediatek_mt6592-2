#ifndef __VKI_XEN_X86_H
#define __VKI_XEN_X86_H

#if defined(__i386__)
#define ___DEFINE_VKI_XEN_GUEST_HANDLE(name, type)			\
    typedef struct { type *p; }						\
        __vki_xen_guest_handle_ ## name;                                \
    typedef struct { union { type *p; vki_xen_uint64_aligned_t q; }; }  \
        __vki_xen_guest_handle_64_ ## name
#define vki_xen_uint64_aligned_t vki_uint64_t __attribute__((aligned(8)))
#define __VKI_XEN_GUEST_HANDLE_64(name) __vki_xen_guest_handle_64_ ## name
#define VKI_XEN_GUEST_HANDLE_64(name) __VKI_XEN_GUEST_HANDLE_64(name)
#else
#define ___DEFINE_VKI_XEN_GUEST_HANDLE(name, type) \
    typedef struct { type *p; } __vki_xen_guest_handle_ ## name
#define vki_xen_uint64_aligned_t vki_uint64_t
#define __DEFINE_VKI_XEN_GUEST_HANDLE(name, type) \
    ___DEFINE_VKI_XEN_GUEST_HANDLE(name, type);   \
    ___DEFINE_VKI_XEN_GUEST_HANDLE(const_##name, const type)
#define DEFINE_VKI_XEN_GUEST_HANDLE(name)   __DEFINE_VKI_XEN_GUEST_HANDLE(name, name)
#define VKI_XEN_GUEST_HANDLE_64(name) VKI_XEN_GUEST_HANDLE(name)
#endif

#define __VKI_XEN_GUEST_HANDLE(name)  __vki_xen_guest_handle_ ## name
#define VKI_XEN_GUEST_HANDLE(name)    __VKI_XEN_GUEST_HANDLE(name)

typedef unsigned long vki_xen_pfn_t;
typedef unsigned long vki_xen_ulong_t;

#if defined(__i386__)
struct vki_xen_cpu_user_regs {
    vki_uint32_t ebx;
    vki_uint32_t ecx;
    vki_uint32_t edx;
    vki_uint32_t esi;
    vki_uint32_t edi;
    vki_uint32_t ebp;
    vki_uint32_t eax;
    vki_uint16_t error_code;    
    vki_uint16_t entry_vector;  
    vki_uint32_t eip;
    vki_uint16_t cs;
    vki_uint8_t  saved_upcall_mask;
    vki_uint8_t  _pad0;
    vki_uint32_t eflags;        
    vki_uint32_t esp;
    vki_uint16_t ss, _pad1;
    vki_uint16_t es, _pad2;
    vki_uint16_t ds, _pad3;
    vki_uint16_t fs, _pad4;
    vki_uint16_t gs, _pad5;
};
#else
struct vki_xen_cpu_user_regs {
    vki_uint64_t r15;
    vki_uint64_t r14;
    vki_uint64_t r13;
    vki_uint64_t r12;
    vki_uint64_t rbp;
    vki_uint64_t rbx;
    vki_uint64_t r11;
    vki_uint64_t r10;
    vki_uint64_t r9;
    vki_uint64_t r8;
    vki_uint64_t rax;
    vki_uint64_t rcx;
    vki_uint64_t rdx;
    vki_uint64_t rsi;
    vki_uint64_t rdi;
    vki_uint32_t error_code;    
    vki_uint32_t entry_vector;  
    vki_uint64_t rip;
    vki_uint16_t cs, _pad0[1];
    vki_uint8_t  saved_upcall_mask;
    vki_uint8_t  _pad1[3];
    vki_uint64_t rflags;      
    vki_uint64_t rsp;
    vki_uint16_t ss, _pad2[3];
    vki_uint16_t es, _pad3[3];
    vki_uint16_t ds, _pad4[3];
    vki_uint16_t fs, _pad5[3]; 
    vki_uint16_t gs, _pad6[3]; 
};
#endif

struct vki_xen_trap_info {
    vki_uint8_t   vector;  
    vki_uint8_t   flags;   
    vki_uint16_t  cs;      
    unsigned long address; 
};

struct vki_xen_vcpu_guest_context {
    
    struct { char x[512]; } fpu_ctxt;       
    unsigned long flags;                    
    struct vki_xen_cpu_user_regs user_regs; 
    struct vki_xen_trap_info trap_ctxt[256];
    unsigned long ldt_base, ldt_ents;       
    unsigned long gdt_frames[16], gdt_ents; 
    unsigned long kernel_ss, kernel_sp;     
    
    unsigned long ctrlreg[8];               
    unsigned long debugreg[8];              
#ifdef __i386__
    unsigned long event_callback_cs;        
    unsigned long event_callback_eip;
    unsigned long failsafe_callback_cs;     
    unsigned long failsafe_callback_eip;
#else
    unsigned long event_callback_eip;
    unsigned long failsafe_callback_eip;
    unsigned long syscall_callback_eip;
#endif
    unsigned long vm_assist;                
#ifdef __x86_64__
    
    vki_uint64_t  fs_base;
    vki_uint64_t  gs_base_kernel;
    vki_uint64_t  gs_base_user;
#endif
};
typedef struct vki_xen_vcpu_guest_context vki_xen_vcpu_guest_context_t;
DEFINE_VKI_XEN_GUEST_HANDLE(vki_xen_vcpu_guest_context_t);


# define VKI_DECLARE_HVM_SAVE_TYPE(_x, _code, _type)                         \
    struct __VKI_HVM_SAVE_TYPE_##_x { _type t; char c[_code]; char cpt[1];}

#define VKI_HVM_SAVE_TYPE(_x) typeof (((struct __VKI_HVM_SAVE_TYPE_##_x *)(0))->t)
#define VKI_HVM_SAVE_LENGTH(_x) (sizeof (VKI_HVM_SAVE_TYPE(_x)))
#define VKI_HVM_SAVE_CODE(_x) (sizeof (((struct __VKI_HVM_SAVE_TYPE_##_x *)(0))->c))

struct vki_hvm_hw_cpu {
   vki_uint8_t  fpu_regs[512];

   vki_uint64_t rax;
   vki_uint64_t rbx;
   vki_uint64_t rcx;
   vki_uint64_t rdx;
   vki_uint64_t rbp;
   vki_uint64_t rsi;
   vki_uint64_t rdi;
   vki_uint64_t rsp;
   vki_uint64_t r8;
   vki_uint64_t r9;
   vki_uint64_t r10;
   vki_uint64_t r11;
   vki_uint64_t r12;
   vki_uint64_t r13;
   vki_uint64_t r14;
   vki_uint64_t r15;

   vki_uint64_t rip;
   vki_uint64_t rflags;

   vki_uint64_t cr0;
   vki_uint64_t cr2;
   vki_uint64_t cr3;
   vki_uint64_t cr4;

   vki_uint64_t dr0;
   vki_uint64_t dr1;
   vki_uint64_t dr2;
   vki_uint64_t dr3;
   vki_uint64_t dr6;
   vki_uint64_t dr7;

   vki_uint32_t cs_sel;
   vki_uint32_t ds_sel;
   vki_uint32_t es_sel;
   vki_uint32_t fs_sel;
   vki_uint32_t gs_sel;
   vki_uint32_t ss_sel;
   vki_uint32_t tr_sel;
   vki_uint32_t ldtr_sel;

   vki_uint32_t cs_limit;
   vki_uint32_t ds_limit;
   vki_uint32_t es_limit;
   vki_uint32_t fs_limit;
   vki_uint32_t gs_limit;
   vki_uint32_t ss_limit;
   vki_uint32_t tr_limit;
   vki_uint32_t ldtr_limit;
   vki_uint32_t idtr_limit;
   vki_uint32_t gdtr_limit;

   vki_uint64_t cs_base;
   vki_uint64_t ds_base;
   vki_uint64_t es_base;
   vki_uint64_t fs_base;
   vki_uint64_t gs_base;
   vki_uint64_t ss_base;
   vki_uint64_t tr_base;
   vki_uint64_t ldtr_base;
   vki_uint64_t idtr_base;
   vki_uint64_t gdtr_base;

   vki_uint32_t cs_arbytes;
   vki_uint32_t ds_arbytes;
   vki_uint32_t es_arbytes;
   vki_uint32_t fs_arbytes;
   vki_uint32_t gs_arbytes;
   vki_uint32_t ss_arbytes;
   vki_uint32_t tr_arbytes;
   vki_uint32_t ldtr_arbytes;

   vki_uint64_t sysenter_cs;
   vki_uint64_t sysenter_esp;
   vki_uint64_t sysenter_eip;

    
   vki_uint64_t shadow_gs;

    
   vki_uint64_t msr_flags;
   vki_uint64_t msr_lstar;
   vki_uint64_t msr_star;
   vki_uint64_t msr_cstar;
   vki_uint64_t msr_syscall_mask;
   vki_uint64_t msr_efer;
   vki_uint64_t msr_tsc_aux;

    
   vki_uint64_t tsc;

    
    union {
       vki_uint32_t pending_event;
        struct {
           vki_uint8_t  pending_vector:8;
           vki_uint8_t  pending_type:3;
           vki_uint8_t  pending_error_valid:1;
           vki_uint32_t pending_reserved:19;
           vki_uint8_t  pending_valid:1;
        };
    };
    
   vki_uint32_t error_code;
};

VKI_DECLARE_HVM_SAVE_TYPE(CPU, 2, struct vki_hvm_hw_cpu);

#endif 

