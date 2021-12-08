/*
 * joystick functions
 *
 * Copyright 1997 Andreas Mohr
 * Copyright 2000 Wolfgang Schwotzer
 * Copyright 2002 David Hagood
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * NOTES:
 *
 * - nearly all joystick functions can be regarded as obsolete,
 *   as Linux (2.1.x) now supports extended joysticks with a completely
 *   new joystick driver interface
 *   New driver's documentation says:
 *     "For backward compatibility the old interface is still included,
 *     but will be dropped in the future."
 *   Thus we should implement the new interface and at most keep the old
 *   routines for backward compatibility.
 * - better support of enhanced joysticks (Linux 2.2 interface is available)
 * - support more joystick drivers (like the XInput extension)
 * - should load joystick DLL as any other driver (instead of hardcoding)
 *   the driver's name, and load it as any low lever driver.
 */

#include "config.h"
#include "wine/port.h"

#ifdef HAVE_LINUX_22_JOYSTICK_API

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_LINUX_IOCTL_H
#include <linux/ioctl.h>
#endif
#include <linux/joystick.h>
#ifdef SW_MAX
#undef SW_MAX
#endif
#define JOYDEV_NEW "/dev/input/js%d"
#define JOYDEV_OLD "/dev/js%d"
#include <errno.h>

#include "joystick.h"

#include "wingdi.h"
#include "winnls.h"
#include "wine/debug.h"

#include "wine/unicode.h"

#ifdef HAVE_SDL_H
# include <SDL.h>
#endif

WINE_DEFAULT_DEBUG_CHANNEL(joystick);

#define MAXJOYSTICK (JOYSTICKID2 + 30)

typedef struct tagWINE_JSTCK {
    int		joyIntf;
    BOOL        in_use;

    int         id; //SDL2 GameController ID
    int         x;
    int         y;
    int         z;
    int         r;
    int         u;
    int         v;
    int         pov_x;
    int         pov_y;
    int         buttons;
    char        axesMap[ABS_MAX + 1];
} WINE_JSTCK;

static	WINE_JSTCK	JSTCK_Data[MAXJOYSTICK];
SDL_GameController *joysticks[MAXJOYSTICK] = NULL;

/**************************************************************************
 * 				JSTCK_drvGet			[internal]
 */
static WINE_JSTCK *JSTCK_drvGet(DWORD_PTR dwDevID)
{
    int	p;

    if ((dwDevID - (DWORD_PTR)JSTCK_Data) % sizeof(JSTCK_Data[0]) != 0)
	return NULL;
    p = (dwDevID - (DWORD_PTR)JSTCK_Data) / sizeof(JSTCK_Data[0]);
    if (p < 0 || p >= MAXJOYSTICK || !((WINE_JSTCK*)dwDevID)->in_use)
	return NULL;

    return (WINE_JSTCK*)dwDevID;
}

/**************************************************************************
 * 				driver_open
 */
LRESULT driver_open(LPSTR str, DWORD dwIntf)
{
    if (dwIntf >= MAXJOYSTICK || JSTCK_Data[dwIntf].in_use)
	return 0;

    JSTCK_Data[dwIntf].joyIntf = dwIntf;
    JSTCK_Data[dwIntf].id = -1;
    JSTCK_Data[dwIntf].in_use = TRUE;
    return (LRESULT)&JSTCK_Data[dwIntf];
}

/**************************************************************************
 * 				driver_close
 */
LRESULT driver_close(DWORD_PTR dwDevID)
{
    WINE_JSTCK*	jstck = JSTCK_drvGet(dwDevID);

    if (jstck == NULL)
	return 0;
    jstck->in_use = FALSE;
    if (jstck->dev > 0)
    {
        SDL_GameControllerClose(joysticks[jstck->dev]);
       jstck->dev = 0;
    }

    return 1;
}

struct js_status
{
    int buttons;
    int x;
    int y;
};

/**************************************************************************
 *                              JSTCK_OpenDevice           [internal]
 */
static	int	JSTCK_OpenDevice(WINE_JSTCK* jstick)
{
    char  buf[20];
    int   flags, fd, found_ix, i;
    static DWORD last_attempt;
    DWORD now;

    if (jstick->id > 0)
      return jstick->id;

    now = GetTickCount();
    if (now - last_attempt < 2000)
      return -1;
    last_attempt = now;

    if (SDL_IsGameController(jstck->id)) {
        joysticks[jstck->id] = SDL_GameControllerOpen(jstck->id);
        if (joysticks[jstck->id]) {
            fprintf("Located controller device at SDL:%i", jstick->id);
            break;
        } else {
            fprintf(stderr, "Could not open gamecontroller %i: %s\n", jstick->id, SDL_GetError());
        }
    }

    return jstick->id;
}


/**************************************************************************
 * 				JoyGetDevCaps		[MMSYSTEM.102]
 */
LRESULT driver_joyGetDevCaps(DWORD_PTR dwDevID, LPJOYCAPSW lpCaps, DWORD dwSize)
{
    WINE_JSTCK*	jstck;
    int		dev;
    char	nrOfAxes;
    char	nrOfButtons;
    char	identString[MAXPNAMELEN];
    int		i;
    int		driverVersion;

    if ((jstck = JSTCK_drvGet(dwDevID)) == NULL)
	return MMSYSERR_NODRIVER;

    if ((dev = JSTCK_OpenDevice(jstck)) < 0) return JOYERR_PARMS;

    return JOYERR_NOERROR;
}

/**************************************************************************
 * 				driver_joyGetPos
 */
LRESULT driver_joyGetPosEx(DWORD_PTR dwDevID, LPJOYINFOEX lpInfo)
{

    return JOYERR_NOERROR;
}

/**************************************************************************
 * 				driver_joyGetPos
 */
LRESULT driver_joyGetPos(DWORD_PTR dwDevID, LPJOYINFO lpInfo)
{

    return ret;
}

#endif /* HAVE_LINUX_JOYSTICK_H */
