/*! \file
*               Author: mstewart
*   \brief      Persistance  module
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "PERSIST_BLK.h"

/* Module Identification for assert functionality */
#define STDC_MODULE_ID PERSIST_BLK

const PERSIST_generic_data_blk_st PERSIST_GENERIC_DEFAULT_DATA_BLK_s =
{
	23u,
    FALSE,
    0u,
    FALSE,
    40,
    0u
};

PERSIST_generic_data_blk_st PERSIST_generic_data_blk_g;

/****************************** END OF FILE *******************************************************/
