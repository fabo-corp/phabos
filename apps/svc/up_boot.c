/************************************************************************************
 *
 *   Copyright (C) 2011-2013 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ************************************************************************************/
/*
 * Copyright (c) 2015 Google, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * * may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/************************************************************************************
 * Included Files
 ************************************************************************************/

#include <nuttx/config.h>
#include <ara_build_info.h>

#include <debug.h>

#define DBG_COMP DBG_DBG
#include "up_debug.h"

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/
/* Configuration ********************************************************************/
/* Should we initialize the NX server?  This is done for NxWidgets (CONFIG_NXWIDGETS=y)
 * if nx_start() is available (CONFIG_NX_NXSTART=y) and if the NxWidget::CNxServer
 * class expects the RTOS to do the NX initialization (CONFIG_NXWIDGET_SERVERINIT=n).
 * This combination of settings is normally only used in the kernel build mode
 * (CONFIG_BUILD_PROTECTED) when NxWidgets is unable to initialize NX from user-space.
 */

#undef HAVE_NXSTART

#if !defined(CONFIG_NX_MULTIUSER)
#  undef CONFIG_NX_START
#endif

#if defined(CONFIG_NXWIDGETS)
#  if defined(CONFIG_NX_NXSTART)
#    if !defined(CONFIG_NXWIDGET_SERVERINIT)
#      define HAVE_NXSTART
#      include <nuttx/nx/nx.h>
#    endif
#  else
#    if !defined(CONFIG_NXWIDGET_SERVERINIT) && defined(CONFIG_BUILD_PROTECTED)
#      error CONFIG_NX_NXSTART=y is needed
#    endif
#  endif
#endif

/* Should we initialize the touchscreen for the NxWM (CONFIG_NXWM=y)?  This
 * is done if we have a touchscreen (CONFIG_INPUT_STMPE811=y), NxWM uses the
 * touchscreen (CONFIG_NXWM_TOUCHSCREEN=y), and if we were asked to
 * initialize the touchscreen for NxWM (NXWM_TOUCHSCREEN_DEVINIT=n). This
 * combination of settings is normally only used in the kernel build mode
 * (CONFIG_BUILD_PROTECTED) when NxWidgets is unable to initialize NX from
 * user-space.
 */

#undef HAVE_TCINIT

#if defined(CONFIG_NXWM_TOUCHSCREEN)
#  if !defined(CONFIG_NXWM_TOUCHSCREEN_DEVNO)
#    error CONFIG_NXWM_TOUCHSCREEN_DEVNO is not defined
#  elif defined(CONFIG_INPUT_STMPE811)
#    if !defined(CONFIG_NXWM_TOUCHSCREEN_DEVINIT)
#      define HAVE_TCINIT
#      include <nuttx/input/touchscreen.h>
#    endif
#  else
#    if !defined(CONFIG_NXWM_TOUCHSCREEN_DEVINIT) && defined(CONFIG_BUILD_PROTECTED)
#      error CONFIG_INPUT_STMPE811=y is needed
#    endif
#  endif
#endif

/* Check if we will need to support the initialization kernel thread */

#undef HAVE_INITTHREAD

#ifdef CONFIG_BOARD_INITIALIZE
#  if defined(CONFIG_NSH_LIBRARY) && !defined(CONFIG_NSH_ARCHINIT)
#    define HAVE_INITTHREAD 1
#  elif defined(HAVE_NXSTART)
#    define HAVE_INITTHREAD 1
#  elif defined(HAVE_TCINIT)
#    define HAVE_INITTHREAD 1
#  endif
#endif

#ifdef HAVE_INITTHREAD
#  include <stdlib.h>
#  include <assert.h>
#  include <nuttx/kthread.h>
#  ifndef CONFIG_STM3240G_BOARDINIT_PRIO
#    define CONFIG_STM3240G_BOARDINIT_PRIO 196
#  endif
#  ifndef CONFIG_STM3240G_BOARDINIT_STACK
#    define CONFIG_STM3240G_BOARDINIT_STACK 2048
#  endif
#endif

/************************************************************************************
 * Private Functions
 ************************************************************************************/

/************************************************************************************
 * Name: board_initthread
 *
 * Description:
 *   Board initialization kernel thread.  This thread exists to support
 *   initialization when CONFIG_BOARD_INITIALIZE is defined.  It is started by
 *   board_initialize() which runs on the IDLE thread.
 *
 *   This function thread exists because some initialization steps may require
 *   waiting for events.  Such waiting is not possible on the IDLE thread.
 *
 * Input Parameters:
 *   Standard task start-up parameters (none of which are used)
 *
 * Returned Value:
 *   Always returns EXIT_SUCCESS.
 *
 ************************************************************************************/

#ifdef HAVE_INITTHREAD
static int board_initthread(int argc, char *argv[])
{
	int ret;

	/* Perform NSH initialization here instead of from the NSH.  This
	 * alternative NSH initialization is necessary when NSH is ran in user-space
	 * but the initialization function must run in kernel space.
	 */

#if defined(CONFIG_NSH_LIBRARY) && !defined(CONFIG_NSH_ARCHINIT)
	ret = nsh_archinitialize();
	if (ret < 0)
	{
		gdbg("ERROR: nsh_archinitialize failed: %d\n", ret);
	}
#endif
	return EXIT_SUCCESS;
}
#endif

void stm32_boardinitialize(void)
{
    ara_show_build_info();
}

/****************************************************************************
 * Name: board_initialize
 *
 * Description:
 *   If CONFIG_BOARD_INITIALIZE is selected, then an additional
 *   initialization call will be performed in the boot-up sequence to a
 *   function called board_initialize().  board_initialize() will be
 *   called immediately after up_initialize() is called and just before the
 *   initial application is started.  This additional initialization phase
 *   may be used, for example, to initialize board-specific device drivers.
 *
 ****************************************************************************/

#ifdef CONFIG_BOARD_INITIALIZE
void board_initialize(void)
{
#ifdef HAVE_INITTHREAD
	pid_t server;

	/* Start the board initialization kernel thread */

	server = kernel_thread("Board Init", CONFIG_STM3240G_BOARDINIT_PRIO,
			 CONFIG_STM3240G_BOARDINIT_STACK, board_initthread,
			 NULL);
	ASSERT(server > 0);
#endif
}
#endif
