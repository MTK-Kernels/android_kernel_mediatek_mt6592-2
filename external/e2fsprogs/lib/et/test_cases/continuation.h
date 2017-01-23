
#include <et/com_err.h>

#define CHPASS_UTIL_PASSWORD_IN_DICTIONARY       (43787520L)
extern const struct error_table et_ovk_error_table;
extern void initialize_ovk_error_table(void);

extern void initialize_ovk_error_table_r(struct et_list **list);

#define ERROR_TABLE_BASE_ovk (43787520L)

#define init_ovk_err_tbl initialize_ovk_error_table
#define ovk_err_base ERROR_TABLE_BASE_ovk
