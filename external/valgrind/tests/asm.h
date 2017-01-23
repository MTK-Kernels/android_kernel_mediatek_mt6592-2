

#if defined(VGO_darwin)
#define VG_SYM(x) "_"#x
#else
#define VG_SYM(x) #x
#endif

#if defined(VGO_darwin)
#define VG_SYM_ASM(x) _##x
#else
#define VG_SYM_ASM(x) x
#endif
