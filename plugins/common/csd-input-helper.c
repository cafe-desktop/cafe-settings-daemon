/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <cdk/cdk.h>
#include <cdk/cdkx.h>

#include <sys/types.h>
#include <X11/Xatom.h>

#include "csd-input-helper.h"

gboolean
supports_xinput_devices (void)
{
        gint op_code, event, error;

        return XQueryExtension (CDK_DISPLAY_XDISPLAY (cdk_display_get_default ()),
                                "XInputExtension",
                                &op_code,
                                &event,
                                &error);
}

static gboolean
device_has_property (XDevice    *device,
                     const char *property_name)
{
        CdkDisplay *display;
        Atom realtype, prop;
        int realformat;
        unsigned long nitems, bytes_after;
        unsigned char *data;

        display = cdk_display_get_default ();

        prop = XInternAtom (CDK_DISPLAY_XDISPLAY (display), property_name, True);
        if (!prop)
                return FALSE;

        cdk_x11_display_error_trap_push (display);
        if ((XGetDeviceProperty (CDK_DISPLAY_XDISPLAY (display), device, prop, 0, 1, False,
                                XA_INTEGER, &realtype, &realformat, &nitems,
                                &bytes_after, &data) == Success) && (realtype != None)) {
                cdk_x11_display_error_trap_pop_ignored (display);
                XFree (data);
                return TRUE;
        }

        cdk_x11_display_error_trap_pop_ignored (display);
        return FALSE;
}

XDevice*
device_is_touchpad (XDeviceInfo *deviceinfo)
{
        CdkDisplay *display;
        XDevice *device;

        display = cdk_display_get_default ();

        if (deviceinfo->type != XInternAtom (CDK_DISPLAY_XDISPLAY (display), XI_TOUCHPAD, True))
                return NULL;

        cdk_x11_display_error_trap_push (display);
        device = XOpenDevice (CDK_DISPLAY_XDISPLAY (display), deviceinfo->id);
        if (cdk_x11_display_error_trap_pop (display) || (device == NULL))
                return NULL;

        if (device_has_property (device, "libinput Tapping Enabled") ||
            device_has_property (device, "Synaptics Off")) {
                return device;
        }

        XCloseDevice (CDK_DISPLAY_XDISPLAY (display), device);
        return NULL;
}

gboolean
touchpad_is_present (void)
{
        XDeviceInfo *device_info;
        gint n_devices;
        guint i;
        gboolean retval;

        if (supports_xinput_devices () == FALSE)
                return TRUE;

        retval = FALSE;

        device_info = XListInputDevices (CDK_DISPLAY_XDISPLAY (cdk_display_get_default ()), &n_devices);
        if (device_info == NULL)
                return FALSE;

        for (i = 0; i < n_devices; i++) {
                XDevice *device;

                device = device_is_touchpad (&device_info[i]);
                if (device != NULL) {
                        retval = TRUE;
                        break;
                }
        }
        if (device_info != NULL)
                XFreeDeviceList (device_info);

        return retval;
}
