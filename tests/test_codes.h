/* -*- c -*- */
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#ifndef _TEST_CODES_H_
#define _TEST_CODES_H_

/* Return this if the test passed. */
#define TEST_PASSED	0

/* Return this if the test could not be run because of some failed
 * pre-condition.  Example: test is run on one node and needs to be run on
 * at least 2. */
#define TEST_NOT_RUN	-1

/* Return this if the test failed for a reason not related to ice-t.
 * Example: could not read or write to a file.  Note that out of memory
 * errors may be caused by ice-t memory leaks. */
#define TEST_NOT_PASSED	-2

/* Return this if the test failed outright. */
#define TEST_FAILED	-3

#endif /*_TEST_CODES_H_*/
