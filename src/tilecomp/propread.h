/* Id */
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#ifndef _PROPREAD_H_
#define _PROPREAD_H_

#ifdef __cplusplus
extern "C" {
#endif

struct property {
    char *name;
    char *value;
};

struct property *propread_read(const char *filename);

void propread_free(struct property *proparray);

struct property *propread_enumerate(struct property *proparray);

#ifdef __cplusplus
}
#endif

#endif /* _PROPREAD_H_*/
