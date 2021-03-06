/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager -- Network link manager
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright 2013 Red Hat, Inc.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "nm-device-tun.h"
#include "nm-device-private.h"
#include "nm-default.h"
#include "nm-platform.h"
#include "nm-device-factory.h"

#include "nmdbus-device-tun.h"

#include "nm-device-logging.h"
_LOG_DECLARE_SELF(NMDeviceTun);

G_DEFINE_TYPE (NMDeviceTun, nm_device_tun, NM_TYPE_DEVICE_GENERIC)

#define NM_DEVICE_TUN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_DEVICE_TUN, NMDeviceTunPrivate))

typedef struct {
	NMPlatformTunProperties props;
	const char *mode;
} NMDeviceTunPrivate;

enum {
	PROP_0,
	PROP_OWNER,
	PROP_GROUP,
	PROP_FLAGS,
	PROP_MODE,
	PROP_NO_PI,
	PROP_VNET_HDR,
	PROP_MULTI_QUEUE,

	LAST_PROP
};

static void
reload_tun_properties (NMDeviceTun *self)
{
	NMDeviceTunPrivate *priv = NM_DEVICE_TUN_GET_PRIVATE (self);
	GObject *object = G_OBJECT (self);
	NMPlatformTunProperties props;

	if (!nm_platform_tun_get_properties (NM_PLATFORM_GET, nm_device_get_ifindex (NM_DEVICE (self)), &props)) {
		_LOGD (LOGD_HW, "could not read tun properties");
		return;
	}

	g_object_freeze_notify (object);

	if (priv->props.owner != props.owner)
		g_object_notify (object, NM_DEVICE_TUN_OWNER);
	if (priv->props.group != props.group)
		g_object_notify (object, NM_DEVICE_TUN_GROUP);
	if (priv->props.no_pi != props.no_pi)
		g_object_notify (object, NM_DEVICE_TUN_NO_PI);
	if (priv->props.vnet_hdr != props.vnet_hdr)
		g_object_notify (object, NM_DEVICE_TUN_VNET_HDR);
	if (priv->props.multi_queue != props.multi_queue)
		g_object_notify (object, NM_DEVICE_TUN_MULTI_QUEUE);

	memcpy (&priv->props, &props, sizeof (NMPlatformTunProperties));

	g_object_thaw_notify (object);
}

static void
link_changed (NMDevice *device, NMPlatformLink *info)
{
	NM_DEVICE_CLASS (nm_device_tun_parent_class)->link_changed (device, info);

	reload_tun_properties (NM_DEVICE_TUN (device));
}

static void
setup (NMDevice *device, NMPlatformLink *plink)
{
	NMDeviceTun *self = NM_DEVICE_TUN (device);
	NMDeviceTunPrivate *priv = NM_DEVICE_TUN_GET_PRIVATE (self);

	NM_DEVICE_CLASS (nm_device_tun_parent_class)->setup (device, plink);

	priv->mode = NULL;
	if (plink->type == NM_LINK_TYPE_TUN)
		priv->mode = "tun";
	else if (plink->type == NM_LINK_TYPE_TAP)
		priv->mode = "tap";
	else
		g_assert_not_reached ();
	g_object_notify (G_OBJECT (device), NM_DEVICE_TUN_MODE);

	reload_tun_properties (NM_DEVICE_TUN (device));
}

/**************************************************************/

static void
nm_device_tun_init (NMDeviceTun *self)
{
}

static void
get_property (GObject *object, guint prop_id,
              GValue *value, GParamSpec *pspec)
{
	NMDeviceTun *self = NM_DEVICE_TUN (object);
	NMDeviceTunPrivate *priv = NM_DEVICE_TUN_GET_PRIVATE (self);

	switch (prop_id) {
	case PROP_OWNER:
		g_value_set_int64 (value, priv->props.owner);
		break;
	case PROP_GROUP:
		g_value_set_int64 (value, priv->props.group);
		break;
	case PROP_MODE:
		g_value_set_string (value, priv->mode);
		break;
	case PROP_NO_PI:
		g_value_set_boolean (value, priv->props.no_pi);
		break;
	case PROP_VNET_HDR:
		g_value_set_boolean (value, priv->props.vnet_hdr);
		break;
	case PROP_MULTI_QUEUE:
		g_value_set_boolean (value, priv->props.multi_queue);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
set_property (GObject *object, guint prop_id,
              const GValue *value, GParamSpec *pspec)
{
	NMDeviceTun *self = NM_DEVICE_TUN (object);
	NMDeviceTunPrivate *priv = NM_DEVICE_TUN_GET_PRIVATE (self);
	const char *str;

	switch (prop_id) {
	case PROP_MODE:
		/* construct-only */
		str = g_value_get_string (value);

		/* mode is G_PARAM_STATIC_STRINGS */
		if (g_strcmp0 (str, "tun") == 0)
			priv->mode = "tun";
		else if (g_strcmp0 (str, "tap") == 0)
			priv->mode = "tap";
		else
			g_return_if_fail (FALSE);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nm_device_tun_class_init (NMDeviceTunClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	NMDeviceClass *device_class = NM_DEVICE_CLASS (klass);

	g_type_class_add_private (klass, sizeof (NMDeviceTunPrivate));

	object_class->get_property = get_property;
	object_class->set_property = set_property;

	device_class->link_changed = link_changed;
	device_class->setup = setup;

	/* properties */
	g_object_class_install_property
		(object_class, PROP_OWNER,
		 g_param_spec_int64 (NM_DEVICE_TUN_OWNER, "", "",
		                     -1, G_MAXUINT32, -1,
		                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property
		(object_class, PROP_GROUP,
		 g_param_spec_int64 (NM_DEVICE_TUN_GROUP, "", "",
		                     -1, G_MAXUINT32, -1,
		                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property
		(object_class, PROP_MODE,
		 g_param_spec_string (NM_DEVICE_TUN_MODE, "", "",
		                      "tun",
		                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property
		(object_class, PROP_NO_PI,
		 g_param_spec_boolean (NM_DEVICE_TUN_NO_PI, "", "",
		                       FALSE,
		                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property
		(object_class, PROP_VNET_HDR,
		 g_param_spec_boolean (NM_DEVICE_TUN_VNET_HDR, "", "",
		                       FALSE,
		                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property
		(object_class, PROP_MULTI_QUEUE,
		 g_param_spec_boolean (NM_DEVICE_TUN_MULTI_QUEUE, "", "",
		                       FALSE,
		                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	nm_exported_object_class_add_interface (NM_EXPORTED_OBJECT_CLASS (klass),
	                                        NMDBUS_TYPE_DEVICE_TUN_SKELETON,
	                                        NULL);
}


/*************************************************************/

#define NM_TYPE_TUN_FACTORY (nm_tun_factory_get_type ())
#define NM_TUN_FACTORY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NM_TYPE_TUN_FACTORY, NMTunFactory))

static NMDevice *
create_device (NMDeviceFactory *factory,
               const char *iface,
               NMPlatformLink *plink,
               NMConnection *connection,
               gboolean *out_ignore)
{
	return (NMDevice *) g_object_new (NM_TYPE_DEVICE_TUN,
	                                  NM_DEVICE_IFACE, iface,
	                                  NM_DEVICE_TYPE_DESC, "Tun",
	                                  NM_DEVICE_DEVICE_TYPE, NM_DEVICE_TYPE_GENERIC,
	                                  NULL);
}

NM_DEVICE_FACTORY_DEFINE_INTERNAL (TUN, Tun, tun,
	NM_DEVICE_FACTORY_DECLARE_LINK_TYPES (NM_LINK_TYPE_TUN, NM_LINK_TYPE_TAP),
	factory_iface->create_device = create_device;
	)

