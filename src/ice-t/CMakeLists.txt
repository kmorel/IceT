# Id
#
## Copyright 2003 Sandia Coporation
## Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
## license for use of this work by or on behalf of the U.S. Government.
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that this Notice and any statement
## of authorship are reproduced on all copies.
#

SET(ICET_SRCS
	porting.c
	context.c
	state.c
	diagnostics.c
	tiles.c
	projections.c
	draw.c
	image.c
	buckets.c
)

ADD_LIBRARY(icet ${ICET_SRCS})

INSTALL_TARGETS(/lib icet)
