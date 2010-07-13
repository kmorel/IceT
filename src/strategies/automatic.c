/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2010 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 */

#include <IceT.h>

#include <IceTDevDiagnostics.h>
#include <IceTDevImage.h>

static void automaticCompose(IceTInt *compose_group, IceTInt group_size,
                             IceTInt image_dest,
                             IceTImage image);

IceTSingleImageStrategy ICET_SINGLE_IMAGE_STRATEGY_AUTOMATIC
    = { "Automatic", automaticCompose };

static void automaticCompose(IceTInt *compose_group, IceTInt group_size,
                             IceTInt image_dest,
                             IceTImage image)
{
    if (group_size >= 8) {
	icetRaiseDebug("Doing bswap compose");
        ICET_SINGLE_IMAGE_STRATEGY_BSWAP.compose(
                                  compose_group, group_size, image_dest, image);
    } else if (group_size > 0) {
	icetRaiseDebug("Doing tree compose");
        ICET_SINGLE_IMAGE_STRATEGY_TREE.compose(
                                  compose_group, group_size, image_dest, image);
    } else {
	icetRaiseDebug("Clearing pixels");
        icetClearImage(image);
    }
}
