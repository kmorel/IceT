/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2010 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#include <IceT.h>

#include <IceTDevDiagnostics.h>
#include <IceTDevImage.h>
#include <IceTDevStrategySelect.h>

void icetAutomaticCompose(IceTInt *compose_group, IceTInt group_size,
                          IceTInt image_dest,
                          IceTImage image)
{
    if (group_size >= 8) {
	icetRaiseDebug("Doing bswap compose");
        icetInvokeSingleImageStrategy(ICET_SINGLE_IMAGE_STRATEGY_BSWAP,
                                      compose_group, group_size,
                                      image_dest, image);
    } else if (group_size > 0) {
	icetRaiseDebug("Doing tree compose");
        icetInvokeSingleImageStrategy(ICET_SINGLE_IMAGE_STRATEGY_TREE,
                                      compose_group, group_size,
                                      image_dest, image);
    } else {
	icetRaiseDebug("Clearing pixels");
        icetClearImage(image);
    }
}
