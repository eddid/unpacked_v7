/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

/*
 * === v7 main()
 */

#ifndef CS_V7_SRC_MAIN_PUBLIC_H_
#define CS_V7_SRC_MAIN_PUBLIC_H_

#include "v7/src/core_public.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*
 * V7 executable main function.
 *
 * There are various callbacks available:
 *
 * `pre_freeze_init()` and `pre_init()` are optional intialization functions,
 * aimed to export any extra functionality into vanilla v7 engine. They are
 * called after v7 initialization, before executing given files or inline
 * expressions. `pre_freeze_init()` is called before "freezing" v7 state;
 * whereas `pre_init` called afterwards.
 *
 * `post_init()`, if provided, is called after executing files and expressions,
 * before destroying v7 instance and exiting.
 */
int v7_main(int argc, char *argv[], void (*pre_freeze_init)(struct v7 *),
            void (*pre_init)(struct v7 *), void (*post_init)(struct v7 *));

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* CS_V7_SRC_MAIN_PUBLIC_H_ */
