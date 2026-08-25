#ifndef INTEGRATION_STUBS_H
#define INTEGRATION_STUBS_H

#include "FBL.h"
#include "UDS.h"
#include "TIME_MGR.h"

/* fbl_main() needs only these: fbl_config_s carries every FBL_init_func_t/runtime pointer FBL
   calls internally (can_init, cantp_init, display_init, ...), so nothing else defined in
   INTEGRATION_STUBS.c is referenced outside of it - unlike APP's INTEGRATION_STUBS.h, which
   exposes one extern per config struct because app_main() calls each init function itself.
   time_cfg_s is the one exception: TIME_init() is called directly from fbl_main() rather than
   through fbl_config_st (it has to run before FBL_init(), which is what actually consumes
   fbl_config_s), the same reason APP's main.c calls TIME_init(&time_cfg_s) itself. */
extern const UDS_func_p_st  fbl_uds_func_table_s;
extern const fbl_config_st  fbl_config_s;
extern const TIME_cfg_st    time_cfg_s;

#endif /* INTEGRATION_STUBS_H */
