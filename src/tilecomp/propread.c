/* Id */
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#include "propread.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define BUFFERSIZE (1024*10)

static char *trim(const char *s);

struct property *propread_read(const char *filename)
{
    FILE *fd;
    struct property *parray;
    int parraylength;
    int parrayused;
    char buffer[BUFFERSIZE];
    char *line;
    char *name;
    char *value;

    fd = fopen(filename, "r");
    if (fd == NULL) {
	return NULL;
    }

    parraylength = 16;
    parray = (struct property *)malloc(parraylength * sizeof(struct property));
    parray[0].name = NULL;
    parray[0].value = NULL;
    parrayused = 0;

    while (!feof(fd)) {
      /* Get a line of input. */
	if (fgets(buffer, BUFFERSIZE, fd) == NULL) break;
	line = buffer;

      /* Strip off leading whitespace. */
	while (isspace(line[0]) && (line[0] != '\0')) line++;

      /* Ignore comments and blank lines. */
	if (line[0] == '#') continue;
	if (line[0] == '\0') continue;

      /* Find name and value. */
	name = line;
	value = strchr(line, '=');
	if (value == NULL) {
	    fprintf(stderr, "Bad line: %s\n", line);
	    propread_free(parray);
	    return NULL;
	}
	value[0] = '\0';
	value++;

	parray[parrayused].name = trim(name);
	parray[parrayused].value = trim(value);
	parrayused++;
	if (parrayused >= parraylength) {
	    parraylength *= 2;
	    parray = (struct property *)realloc(parray,
						  parraylength
						* sizeof(struct property));
	}
	parray[parrayused].name = NULL;
	parray[parrayused].value = NULL;
    }

    fclose(fd);

    return parray;
}

void propread_free(struct property *proparray)
{
    struct property *pa = proparray;

    while (pa->name != NULL) {
	free(pa->name);
	free(pa->value);
	pa++;
    }

    free(proparray);
}

struct property *propread_enumerate(struct property *proparray)
{
    static struct property *propsave;

    if (proparray != NULL) {
	propsave = proparray;
    } else {
	propsave++;
    }

    if (propsave->name != NULL) {
	return propsave;
    } else {
	return NULL;
    }
}

static char *trim(const char *s)
{
    char *t;
    int start, end;

    start = 0;
    end = strlen(s);
    while (isspace(s[start]) && (start < end)) start++;
    while (isspace(s[end-1]) && (start < end)) end--;

    t = (char *)malloc(end - start + 1);
    strncpy(t, s + start, end-start);
    t[end-start] = '\0';

    return t;
}
