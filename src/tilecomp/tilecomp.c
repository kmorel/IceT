/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

/* Id */

#include <GL/ice-t.h>
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/ice-t_buckets.h>
#include <GL/ice-t_mpi.h>

#include <mpi.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>

#include <gmrd.h>

#include "propread.h"

#define USE_NVIDIA	0
#define FANCY_COLORS	0
#define DUPLICATE_DATA	0

#if USE_NVIDIA
#ifdef __glext_h_
#undef __glext_h_
#undef GL_GLEXT_VERSION
#undef GL_NV_vertex_array_range
#undef GL_NV_fence
#endif
#define GL_GLEXT_PROTOTYPES
#include "glext.h"

/*  extern void glVertexArrayRangeNV(GLsizei, const GLvoid *); */
extern void *glXAllocateMemoryNV(GLsizei, GLfloat, GLfloat, GLfloat);
extern void glXFreeMemoryNV(void *);
/*  extern void glGenFencesNV(GLsizei n, GLuint *fences); */
/*  extern void glSetFenceNV(GLuint fence, GLenum condition); */
/*  extern void glFinishFenceNV(GLuint fence); */
#endif //USE_NVIDIA

static void init(int argc, char **argv);
static void read_properties(void);
static void display(void);
static void project(void);
static void draw(void);
static void draw_bucket(int bucket);
static void idle(void);
static void record_times(double total_time);
static void finalize(void);

static void write_ppm(const char *filename,
		      const unsigned char *image,
		      int width, int height);

#if FANCY_COLORS
static void HSV2RGB(float h, float s, float v,
		    float *r, float *g, float *b);
#endif /*FANCY_COLORS*/

static int width = 250;
static int height = 250;
static int num_tiles_x = 1;
static int num_tiles_y = 1;
static enum { ORTHO, FRUSTUM, PERSPECTIVE } projection_type;
static double ortho_params[6] = { -1.0, 1.0, -1.0, 1.0, -1.0, 1.0 };
static double frustum_params[6] = { -1.0, 1.0, -1.0, 1.0, 1.0, 2.0 };
static double perspective_params[4] = { 90.0, 1.33, 1.0, 2.0 };

static char geomfiles[FILENAME_MAX];
static int *file_nums = NULL;
static struct gmrd_data *gd_array;
static int num_gmrd;
static int total_tri_cnt;
static float total_x_min, total_x_max, total_y_min, total_y_max;
static float max_bound;
static float total_z_min, total_z_max;
static IceTBucket *buckets;

static int write_images = 0;
static char image_file[FILENAME_MAX];

#if USE_NVIDIA
static GLint *fences;
static GLvoid *OnCardMemory;
#endif

static int rank;
static int processors;
static unsigned long frame_count = 0;

static double rotate_deg = 360.0;
static double rotate_axis[3] = { 0.0, 0.0, 1.0 };
/*static double scale = 1.0;
  static double translate[3] = { 0.0, 0.0, 0.0 }; */

static FILE *log_fd;

#define LOGBUFLEN	4095
static char logbuf[LOGBUFLEN+1];
#define LH	logbuf
#define LOG(snprintfargs)			\
    if (rank == 0) {				\
	sprintf snprintfargs;			\
	printf("%s", logbuf);			\
	fflush(stdout);				\
	fprintf(log_fd, "%s", logbuf);		\
	fflush(log_fd);				\
    }
#define LOG0(format)			LOG((LH, format))
#define LOG1(format, arg1)		LOG((LH, format, arg1))
#define LOG2(format, arg1, arg2)	LOG((LH, format, arg1, arg2))
#define LOG3(format, arg1, arg2, arg3)	LOG((LH, format, arg1, arg2, arg3))

int main(int argc, char **argv)
{
    IceTCommunicator comm;
    putenv("DISPLAY=localhost:0");

    MPI_Init(&argc, &argv);
    glutInit(&argc, argv);
    comm = icetCreateMPICommunicator(MPI_COMM_WORLD);
    icetCreateContext(comm);
    icetDestroyMPICommunicator(comm);
    init(argc, argv);
    glutMainLoop();
    return 0;
}

static void init(int argc, char **argv)
{
    GLfloat light_ambient[]	= { 0.3f, 0.3f, 0.3f, 1.0f };
    GLfloat light_diffuse[]	= { 0.4f, 0.4f, 0.4f, 1.0f };
    GLfloat light_specular[]	= { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat light_position[]	= { -1.0f, 1.0f, 1.0f, 0.0f };
#if FANCY_COLORS
    GLfloat light_position2[]	= { 1.0f, -1.0f, -1.0f, 0.0f };
#endif /*FANCY_COLORS*/

#if !FANCY_COLORS
    GLfloat gray_mat[]		= { 0.8f, 0.8f, 0.8f, 1.0f };
    GLfloat red_mat[]		= { 1.0f, 0.0f, 0.0f, 1.0f };
    GLfloat orange_mat[]	= { 1.0f, 0.5f, 0.0f, 1.0f };
    GLfloat yellow_mat[]	= { 1.0f, 1.0f, 0.0f, 1.0f };
    GLfloat yellow_green_mat[]	= { 0.5f, 1.0f, 0.0f, 1.0f };
    GLfloat green_mat[]		= { 0.0f, 1.0f, 0.0f, 1.0f };
    GLfloat turquoise_mat[]	= { 0.0f, 1.0f, 0.5f, 1.0f };
    GLfloat cyan_mat[]		= { 0.0f, 1.0f, 1.0f, 1.0f };
    GLfloat lt_blue_mat[]	= { 0.0f, 0.5f, 1.0f, 1.0f };
    GLfloat blue_mat[]		= { 0.0f, 0.0f, 1.0f, 1.0f };
    GLfloat indigo_mat[]	= { 0.5f, 0.0f, 1.0f, 1.0f };
    GLfloat violet_mat[]	= { 1.0f, 0.0f, 1.0f, 1.0f };
    GLfloat pink_mat[]		= { 1.0f, 0.5f, 0.5f, 1.0f };
    GLfloat khaki_mat[]		= { 0.95f, 0.9f, 0.55f, 1.0f };
    GLfloat brown_mat[]		= { 0.5f, 0.5f, 0.25f, 1.0f };
    GLfloat plum_mat[]		= { 1.0f, 0.7f, 1.0f, 1.0f };
#endif /*!FANCY_COLORS*/
    GLfloat white_mat[]		= { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat shininess_mat[]	= { 100.0f };

    GLfloat *material = NULL;

    char title[256];

    int i, j;

    char *logfilename = "log";
    char myfilename[FILENAME_MAX];

    float x_min, x_max, y_min, y_max, z_min, z_max;

    int max_vert_datalen = 0;
    int max_vert_cnt = 0;

    time_t currenttime;

    icetGetIntegerv(ICET_RANK, &rank);
    icetGetIntegerv(ICET_NUM_PROCESSORS, &processors);

    if (rank == 0) {
	if (argc > 1) {
	    logfilename = argv[1];
	}
	log_fd = fopen(logfilename, "at");
	if (log_fd == NULL) {
	    fprintf(stderr, "Could not open log file: %s.\n", logfilename);
	    exit(-1);
	}
    }

    read_properties();

  /*icetDiagnostics(ICET_DIAG_ERRORS|ICET_DIAG_WARNINGS|ICET_DIAG_DEBUG);*/
    icetDiagnostics(ICET_DIAG_ERRORS|ICET_DIAG_WARNINGS|ICET_DIAG_ALL_NODES);
  /*icetDiagnostics(ICET_DIAG_FULL);*/

    icetDrawFunc(draw);
    for (j = num_tiles_y-1; j >= 0; j--) {
	for (i = 0; i < num_tiles_x; i++) {
	    icetAddTile(width*i, height*j, width, height, j*num_tiles_x + i);
	}
    }
    icetStrategy(ICET_STRATEGY_REDUCE);
  /*icetStrategy(ICET_STRATEGY_SERIAL);*/

    if (icetGetError() != ICET_NO_ERROR) {
	printf("ICET error detected.  Quiting.\n");
	MPI_Finalize();
	exit(-1);
    }

    glutInitWindowSize(width, height);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    sprintf(title, "%d: tilecomp", rank);
    glutCreateWindow(title);
#ifdef _DEBUG
    glutPositionWindow((width+10)*rank, 0);
#else
    glutPositionWindow(0, 0);
#endif
    glutDisplayFunc(display);
    glutIdleFunc(idle);

    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
#if FANCY_COLORS
    glLightfv(GL_LIGHT1, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, light_specular);
    glLightfv(GL_LIGHT1, GL_POSITION, light_position2);
#endif /*FANCY_COLORS*/

#if !FANCY_COLORS
    switch (rank % 16) {
      case 0:
	  material = gray_mat;
	  break;
      case 1:
	  material = red_mat;
	  break;
      case 2:
	  material = orange_mat;
	  break;
      case 3:
	  material = yellow_mat;
	  break;
      case 4:
	  material = yellow_green_mat;
	  break;
      case 5:
	  material = green_mat;
	  break;
      case 6:
	  material = turquoise_mat;
	  break;
      case 7:
	  material = cyan_mat;
	  break;
      case 8:
	  material = lt_blue_mat;
	  break;
      case 9:
	  material = blue_mat;
	  break;
      case 10:
	  material = indigo_mat;
	  break;
      case 11:
	  material = violet_mat;
	  break;
      case 12:
	  material = pink_mat;
	  break;
      case 13:
	  material = khaki_mat;
	  break;
      case 14:
	  material = brown_mat;
	  break;
      case 15:
	  material = plum_mat;
	  break;
    }
#else /*FANCY_COLORS*/
    material = malloc(4*sizeof(float));
    material[0] = 1.0; material[1] = 1.0; material[2] = 1.0; material[3] = 1.0;
#endif /*!FANCY_COLORS*/

    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white_mat);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shininess_mat);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, material);
    material[0] /= 4.0;  material[1] /= 4.0;  material[2] /= 4.0;
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, material);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
#if FANCY_COLORS
    glEnable(GL_LIGHT1);
#endif
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);

    total_tri_cnt = 0;
#if !DUPLICATE_DATA
    num_gmrd = file_nums[rank+1] - file_nums[rank];
#else /*DUPLICATE_DATA*/
    num_gmrd = file_nums[rank/DUPLICATE_DATA+1] - file_nums[rank/DUPLICATE_DATA];
#endif /*!DUPLICATE_DATA*/
    gd_array = malloc(sizeof(struct gmrd_data)*num_gmrd);
    buckets = malloc(sizeof(IceTBucket)*num_gmrd);
    for (i = 0; i < num_gmrd; i++) {
#if !DUPLICATE_DATA
	sprintf(myfilename, geomfiles, i + file_nums[rank]);
#else /*DUPLICATE_DATA*/
	sprintf(myfilename, geomfiles, i + file_nums[rank/DUPLICATE_DATA]);
#endif /*!DUPLICATE_DATA*/
	gd_array[i].filename   = myfilename;
	gd_array[i].processors = 1;
	gd_array[i].rank       = 0;

	if (rank == 0) {
	    printf("\nReading %s...", myfilename);
	    fflush(stdout);
	}

	if (gmrd_read(&gd_array[i]) == -1) {
	    fprintf(stderr, "\n%d: Err reading %s: %s\n", rank,
		    gd_array[i].filename, strerror(errno));
	    exit(1);
	}
	if (   (gd_array[i].type != GL_TRIANGLE_STRIP)
	    && (gd_array[i].type != GL_TRIANGLES) ) {
	    fprintf(stderr,
		    "Only supporting triangle strip or plain triangle data.\n");
	    exit(1);
	}

	if (gd_array[i].type == GL_TRIANGLES) {
	    gd_array[i].vert_datalen = gd_array[i].tri_cnt*72;
	}

	if (0) {
	    GLfloat *normal;
	    if (rank == 0) {
		printf(" flipping normals...");
		fflush(stdout);
	    }
	    for (normal = gd_array[i].vert_data+3;
		 normal < gd_array[i].vert_data
		     + gd_array[i].vert_datalen/sizeof(GLfloat);
		 normal += 6) {
		normal[0] = -normal[0];
		normal[1] = -normal[1];
		normal[2] = -normal[2];
	    }
	}

	if (1) {
	    int *conn = gd_array[i].conn_data;
	    if (rank == 0) {
		printf(" checking integrity...");
		fflush(stdout);
	    }
	    for (j = 0; j < gd_array[i].mytstrip_cnt; j++) {
		int vcnt = *(conn++);
		int k;
		for (k = 0; k < vcnt; k++, conn++) {
		    if ((*conn < 0) || (*conn >= gd_array[i].vert_cnt)) {
			printf("\nBAD VERTEX in %s\n", gd_array[i].filename);
			printf("\ttristrip %d, vertex %d has id %d\n",
			       j, k, *conn);
			exit(1);
		    }
		}
	    }
	}

	if (gd_array[i].vert_datalen > max_vert_datalen) {
	    max_vert_datalen = gd_array[i].vert_datalen;
	}
	if (gd_array[i].vert_cnt > max_vert_cnt) {
	    max_vert_cnt = gd_array[i].vert_cnt;
	}

	total_tri_cnt += gd_array[i].tri_cnt;
	if (i == 0) {
	    x_min = gd_array[0].vert_x_min;
	    x_max = gd_array[0].vert_x_max;
	    y_min = gd_array[0].vert_y_min;
	    y_max = gd_array[0].vert_y_max;
	    z_min = gd_array[0].vert_z_min;
	    z_max = gd_array[0].vert_z_max;
	} else {
	    if (x_min > gd_array[i].vert_x_min) x_min = gd_array[i].vert_x_min;
	    if (x_max < gd_array[i].vert_x_max) x_max = gd_array[i].vert_x_max;
	    if (y_min > gd_array[i].vert_y_min) y_min = gd_array[i].vert_y_min;
	    if (y_max < gd_array[i].vert_y_max) y_max = gd_array[i].vert_y_max;
	    if (z_min > gd_array[i].vert_z_min) z_min = gd_array[i].vert_z_min;
	    if (z_max < gd_array[i].vert_z_max) z_max = gd_array[i].vert_z_max;
	}

	buckets[i] = icetCreateBucket();
	icetBucketBoxd(buckets[i],
		       gd_array[i].vert_x_min, gd_array[i].vert_x_max,
		       gd_array[i].vert_y_min, gd_array[i].vert_y_max,
		       gd_array[i].vert_z_min, gd_array[i].vert_z_max);
    }

    if (rank == 0) {
	printf("done. (%d triangles)\n", total_tri_cnt);
	fflush(stdout);
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
#if USE_NVIDIA
    glEnableClientState(GL_VERTEX_ARRAY_RANGE_NV);

    OnCardMemory = glXAllocateMemoryNV(max_vert_datalen, 1.0, 1.0, 1.0);

    if (OnCardMemory == NULL) {
	printf("Could not allocate %d bytes of on card memory.\n",
	       max_vert_datalen);
	exit(1);
    } else {
	GLboolean valid;
	GLint max_range;
	glGetIntegerv(GL_MAX_VERTEX_ARRAY_RANGE_ELEMENT_NV, &max_range);
	if (max_range < max_vert_cnt) {
	    printf("Maximum vertex array range element too small.\n");
	    printf("%d given, %d needed\n", max_range, max_vert_cnt);
	    exit(1);
	}
      // Test vertex array range.
	glVertexArrayRangeNV(max_vert_datalen, OnCardMemory);
	glGetBooleanv(GL_VERTEX_ARRAY_RANGE_VALID_NV, &valid);
	if (!valid) {
	    printf("Vertex array range is not valid!\n");
	    exit(1);
	}
	glVertexArrayRangeNV(0, NULL);
    }

    fences = malloc(sizeof(GLint)*num_gmrd);
    glGenFencesNV(num_gmrd, fences);
#endif

    icetBoundingBoxf(x_min, x_max, y_min, y_max, z_min, z_max);

    i = total_tri_cnt;
    MPI_Reduce(&i, &total_tri_cnt, 1,
	       MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Allreduce(&x_min, &total_x_min, 1,
		  MPI_FLOAT, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&x_max, &total_x_max, 1,
		  MPI_FLOAT, MPI_MAX, MPI_COMM_WORLD);
    MPI_Allreduce(&y_min, &total_y_min, 1,
		  MPI_FLOAT, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&y_max, &total_y_max, 1,
		  MPI_FLOAT, MPI_MAX, MPI_COMM_WORLD);
    MPI_Allreduce(&z_min, &total_z_min, 1,
		  MPI_FLOAT, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&z_max, &total_z_max, 1,
		  MPI_FLOAT, MPI_MAX, MPI_COMM_WORLD);

#if FANCY_COLORS
    {
	GLfloat contour_plane[4] = {0.0, 0.0, 0.0, 0.0};
	unsigned char *texture;
	unsigned char *t;

      /* This actually tends to slow down things a lot. */
	glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);

	contour_plane[2] = -1.5f/(total_z_max-total_z_min);
	contour_plane[3] = 0.7f + total_z_min/(total_z_max-total_z_min);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      /*glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);*/
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#define TEX_SIZE 512
	texture = malloc(TEX_SIZE*4);
	t = texture;
	for (i = 0; i < TEX_SIZE; i++) {
	    float r, g, b;
/* 	    HSV2RGB((240.0f*i)/TEX_SIZE, ((float)i)/TEX_SIZE, */
/* 		    (float)pow(1.0-((float)i)/TEX_SIZE, 0.2), */
/* 		    &r, &g, &b); */
/* 	    HSV2RGB((240.0f*i)/TEX_SIZE, 0.75, */
/* 		    (float)pow(1.0-((float)i)/TEX_SIZE, 0.2), */
/* 		    &r, &g, &b); */
	    HSV2RGB((240.0f*i)/TEX_SIZE, exp(-(float)i/TEX_SIZE),
		    (float)pow(1.0-((float)i)/TEX_SIZE, 0.2),
		    &r, &g, &b);
/*  	    HSV2RGB((240.0f*i)/TEX_SIZE, 1.0, 1.0, */
/*  		    &r, &g, &b); */
	    t[0] = (unsigned char)(r*255);
	    t[1] = (unsigned char)(g*255);
	    t[2] = (unsigned char)(b*255);
	    t[3] = 255;
	    t += 4;
	}
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, TEX_SIZE, 0, GL_RGBA,
		     GL_UNSIGNED_BYTE, texture);
	free(texture);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, contour_plane);

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_1D);
    }
#endif /*FANCY_COLORS*/

    time(&currenttime);
    LOG1("\nLog entry:   %s\n", ctime(&currenttime));
    LOG1("File:        %s\n", myfilename);
    LOG((LH, "Bounds:       %.3f-%.3f x %.3f-%.3f x %.3f-%.3f\n",
	 x_min, x_max, y_min, y_max, z_min, z_max));
    LOG2("Display:     %dx%d\n", width, height);
    LOG1("Total Tri:   %d\n", total_tri_cnt);
    LOG1("Num Proc:    %d\n", processors);
    LOG1("Num Tiles:   %d\n", num_tiles_x * num_tiles_y);
    LOG1("Rotate:      %f degrees\n", rotate_deg);
    LOG3("About:       %.3f %.3f %.3f\n",
	 rotate_axis[0], rotate_axis[1], rotate_axis[2]);
    LOG1("Strategy:    %s\n", icetGetStrategyName());

    LOG0("\n"
	 "---------------------------------------------\n"
	 "|         (Measured in milliseconds)        |-------------------------------\n"
	 "| Render   Read  Write Cmprss   Cmp  Compose| Net Use   Tri/sec   Frame/sec|\n"
	 "|-------|------|------|------|------|-------|--------|-----------|---------|\n");
}

static void read_properties(void)
{
    struct property *properties;
    struct property *prop;

    properties = propread_read("tilecomp.prop");
    if (properties == NULL) {
	printf("Could not find tilecomp.prop\n");
	exit(1);
    }

    for (prop = propread_enumerate(properties);
	 prop != NULL;
	 prop = propread_enumerate(NULL))
    {
	if (strcmp(prop->name, "NUM_TILES_X") == 0) {
	    num_tiles_x = atoi(prop->value);
	} else if (strcmp(prop->name, "NUM_TILES_Y") == 0) {
	    num_tiles_y = atoi(prop->value);
	} else if (strcmp(prop->name, "WIDTH") == 0) {
	    width = atoi(prop->value);
	} else if (strcmp(prop->name, "HEIGHT") == 0) {
	    height = atoi(prop->value);
	} else if (strcmp(prop->name, "ORTHO_PROJ") == 0) {
	    int i;
	    char *tok = strtok(prop->value, " \t\r\n");
	    projection_type = ORTHO;
	    for (i = 0; i < 6; i++) {
		ortho_params[i] = atof(tok);
		tok = strtok(NULL, " \t\r\n");
	    }
	} else if (strcmp(prop->name, "FRUSTUM") == 0) {
	    int i;
	    char *tok = strtok(prop->value, " \t\r\n");
	    projection_type = FRUSTUM;
	    for (i = 0; i < 6; i++) {
		frustum_params[i] = atof(tok);
		tok = strtok(NULL, " \t\r\n");
	    }
	} else if (strcmp(prop->name, "PERSPECTIVE") == 0) {
	    int i;
	    char *tok = strtok(prop->value, " \t\r\n");
	    projection_type = PERSPECTIVE;
	    for (i = 0; i < 4; i++) {
		perspective_params[i] = atof(tok);
		tok = strtok(NULL, " \t\r\n");
	    }
	} else if (strcmp(prop->name, "GEOMFILES") == 0) {
	    strcpy(geomfiles, prop->value);
	} else if (strcmp(prop->name, "GEOMNUMS") == 0) {
	    int i;
	    char *tok = strtok(prop->value, " \t\r\n");
#if !DUPLICATE_DATA
	    int num_nums = processors + 1;
#else
	    int num_nums = (processors + DUPLICATE_DATA - 1)/DUPLICATE_DATA + 1;
#endif
	    file_nums = (int *)malloc(sizeof(int)*(num_nums));
	    for (i = 0; i < num_nums; i++) {
		file_nums[i] = atoi(tok);
		tok = strtok(NULL, " \t\r\n");
	    }
	} else if (strcmp(prop->name, "ROTATE_DEG") == 0) {
	    rotate_deg = atoi(prop->value);
	} else if (strcmp(prop->name, "ROTATE_AXIS") == 0) {
	    int i;
	    char *tok = strtok(prop->value, " \t\r\n");
	    for (i = 0; i < 3; i++) {
		rotate_axis[i] = (float)atof(tok);
		tok = strtok(NULL, " \t\r\n");
	    }
/*	} else if (strcmp(prop->name, "SCALE") == 0) {
	    scale = (float)atof(prop->value);
	} else if (strcmp(prop->name, "TRANSLATE") == 0) {
	    int i;
	    char *tok = strtok(prop->value, " \t\r\n");
	    for (i = 0; i < 3; i++) {
		translate[i] = (float)atof(tok);
		tok = strtok(NULL, " \t\r\n");
		} */
	} else if (strcmp(prop->name, "IMAGE_FILES") == 0) {
	    write_images = 1;
	    strcpy(image_file, (const char *)prop->value);
	} else {
	    printf("Unknown property %s\n", prop->name);
	}
    }
}

static void display(void)
{
    static GLdouble rot = 0.0;
    double total_time;
    unsigned char *image;

    total_time = MPI_Wtime();

    project();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
#if DUPLICATE_DATA
    glTranslated(0.0, 0.0,
		 -(2.0*(rank%DUPLICATE_DATA) + 1.0)/DUPLICATE_DATA - 1.0);
#else /*!DUPLICATE_DATA*/
    glTranslated(0.0, 0.0, -2.0);
#endif /*DUPLICATE_DATA*/
    glRotated(rot, rotate_axis[0], rotate_axis[1], rotate_axis[2]);
    if (total_x_max-total_x_min > total_y_max-total_y_min) {
	if (total_x_max-total_x_min > total_z_max-total_z_min) {
	    max_bound = total_x_max-total_x_min;
	} else {
	    max_bound = total_z_max-total_z_min;
	}
    } else {
	if (total_y_max-total_y_min > total_z_max-total_z_min) {
	    max_bound = total_y_max-total_y_min;
	} else {
	    max_bound = total_z_max-total_z_min;
	}
    }
    glScalef(2/max_bound, 2/max_bound, 2/max_bound);
    glTranslated(-(total_x_min+total_x_max)/2, -(total_y_min+total_y_max)/2,
		 -(total_z_min+total_z_max)/2);

    icetDrawFrame();
    MPI_Barrier(MPI_COMM_WORLD);
    glutSwapBuffers();

    total_time = MPI_Wtime() - total_time;

    if (write_images) {
	GLint display_tile;
	icetGetIntegerv(ICET_TILE_DISPLAYED, &display_tile);
	if (display_tile >= 0) {
	    char filename[64];
	    GLint width, height;
	    sprintf(filename, image_file,
		    display_tile%num_tiles_x, display_tile/num_tiles_x,
		    frame_count);
	    icetGetIntegerv(ICET_TILE_MAX_WIDTH, &width);
	    icetGetIntegerv(ICET_TILE_MAX_HEIGHT, &height);
	    image = icetGetColorBuffer();
	    write_ppm(filename, image, width, height);
	}
    }

  /* Do not record first frame.  Readings can be erroneous. */
    if (rot != 0.0) {
	record_times(total_time);
    }

    rot += 5.0;
    if (rot >= rotate_deg) {
	finalize();
    }
}

static void project(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    switch (projection_type) {
      case ORTHO:
	  glOrtho(ortho_params[0],
		  ortho_params[1],
		  ortho_params[2],
		  ortho_params[3],
		  ortho_params[4],
		  ortho_params[5]);
	  break;
      case FRUSTUM:
	  glFrustum(frustum_params[0],
		    frustum_params[1],
		    frustum_params[2],
		    frustum_params[3],
		    frustum_params[4],
		    frustum_params[5]);
	  break;
      case PERSPECTIVE:
	  gluPerspective(perspective_params[0],
			 perspective_params[1],
			 perspective_params[2],
			 perspective_params[3]);
	  break;
    }
}

#if USE_NVIDIA
static GLint last_fence;
#endif
static void draw(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#if USE_NVIDIA
    last_fence = -1;
#endif

#if 0
    icetBucketsDraw(buckets, num_gmrd, draw_bucket);
#else
    {
	int i;
	for (i = 0; i < num_gmrd; i++) {
	    draw_bucket(i);
	}
    }
#endif
}

static void draw_bucket(int bucket)
{
    struct gmrd_data *gd = gd_array + bucket;
    int *connp;
    int nverts;
    int i;

#if USE_NVIDIA
  /* Wait for the last stuff to finish */
    if (last_fence != -1) {
	glFinishFenceNV(last_fence);
    }
  /* Preload vertex information so we do not have to wait in between
     glDrawElements calls. */
    memcpy(OnCardMemory, (const void *)gd->vert_data, gd->vert_datalen);
  /* Set up and lock down the new vertex pointers. */
    glNormalPointer(GL_FLOAT, 6*sizeof(float), ((float *)OnCardMemory)+3);
    glVertexPointer(3, GL_FLOAT, 6*sizeof(float), OnCardMemory);
    glVertexArrayRangeNV(gd->vert_datalen, OnCardMemory);
#else /*!USE_NVIDIA*/
    glNormalPointer(GL_FLOAT, 6*sizeof(float), gd->vert_data+3);
    glVertexPointer(3, GL_FLOAT, 6*sizeof(float), gd->vert_data);
#endif /*USE_NVIDIA*/

    if (gd->type == GL_TRIANGLE_STRIP) {

	connp = gd->conn_data;
	for (i = 0; i < gd->mytstrip_cnt; i++) {
/*  	for (i = 0; i < gd->mytstrip_cnt-66; i++) { */
	    nverts = *(connp++);
	    glDrawElements(GL_TRIANGLE_STRIP, nverts, GL_UNSIGNED_INT, connp);
	    connp += nverts;
	}

    } else if (gd->type == GL_TRIANGLES) {

	glDrawArrays(GL_TRIANGLES, 0, gd->mytri_cnt*3);

    }

#if USE_NVIDIA
  /* Set up a fence so we now when we can render the next peice. */
    last_fence = fences[bucket];
    glSetFenceNV(last_fence, GL_ALL_COMPLETED_NV);
#endif
}

static void idle(void)
{
    glutPostRedisplay();
}

static double total_render_time = 0.0;
static double total_read_time = 0.0;
static double total_write_time = 0.0;
static double total_compress_time = 0.0;
static double total_compare_time = 0.0;
static double total_compose_time = 0.0;
static double total_tri_rate = 0.0;
static double total_frame_rate = 0.0;
static double total_mbytes_trans = 0;

static void record_times(double total_time)
{
    GLdouble local_render_time;
    GLdouble local_buf_read_time;
    GLdouble local_buf_write_time;
    GLdouble local_compress_time;
    GLdouble local_compare_time;
    GLdouble local_compose_time;
    GLdouble local_bytes_sent;
    GLdouble render_time;
    GLdouble buf_read_time;
    GLdouble buf_write_time;
    GLdouble compress_time;
    GLdouble compare_time;
    GLdouble compose_time;
    GLdouble mbytes_trans;

    icetGetDoublev(ICET_RENDER_TIME, &local_render_time);
    icetGetDoublev(ICET_BUFFER_READ_TIME, &local_buf_read_time);
    icetGetDoublev(ICET_BUFFER_WRITE_TIME, &local_buf_write_time);
    icetGetDoublev(ICET_COMPRESS_TIME, &local_compress_time);
    icetGetDoublev(ICET_COMPARE_TIME, &local_compare_time);
    icetGetDoublev(ICET_COMPOSITE_TIME, &local_compose_time);
    icetGetDoublev(ICET_BYTES_SENT, &local_bytes_sent);

    MPI_Reduce(&local_render_time, &render_time, 1, MPI_DOUBLE, MPI_MAX,
	       0, MPI_COMM_WORLD);
    MPI_Reduce(&local_buf_read_time, &buf_read_time, 1, MPI_DOUBLE, MPI_MAX,
	       0, MPI_COMM_WORLD);
    MPI_Reduce(&local_buf_write_time, &buf_write_time, 1, MPI_DOUBLE, MPI_MAX,
	       0, MPI_COMM_WORLD);
    MPI_Reduce(&local_compress_time, &compress_time, 1, MPI_DOUBLE, MPI_MAX,
	       0, MPI_COMM_WORLD);
    MPI_Reduce(&local_compare_time, &compare_time, 1, MPI_DOUBLE, MPI_MAX,
	       0, MPI_COMM_WORLD);
    MPI_Reduce(&local_compose_time, &compose_time, 1, MPI_DOUBLE, MPI_MAX,
	       0, MPI_COMM_WORLD);
    MPI_Reduce(&local_bytes_sent, &mbytes_trans, 1, MPI_DOUBLE, MPI_SUM,
	       0, MPI_COMM_WORLD);
    mbytes_trans /= 1024*1024;

    LOG((LH, "%8.1f %6.1f %6.1f %6.1f %6.1f %6.1f %5.0f MB %10.3fM %6.3f Hz\n",
	 render_time*1000.0, buf_read_time*1000.0, buf_write_time*1000.0,
	 compress_time*1000.0, compare_time*1000.0, compose_time*1000.0,
	 mbytes_trans, total_tri_cnt/(total_time*1000000.0), 1.0/total_time));

    total_render_time += render_time;
    total_read_time += buf_read_time;
    total_write_time += buf_write_time;
    total_compress_time += compress_time;
    total_compare_time += compare_time;
    total_compose_time += compose_time;
    total_tri_rate += total_tri_cnt/total_time;
    total_frame_rate += 1.0/total_time;
    total_mbytes_trans += mbytes_trans;
    frame_count++;
}

static void finalize(void)
{
    MPI_Finalize();
    glutDestroyWindow(glutGetWindow());

    if (frame_count < 1) exit(0);

    LOG0("|-------|------|------|------|------|-------|--------|-----------|---------|\n");
    LOG((LH, "%8.1f %6.1f %6.1f %6.1f %6.1f %6.1f %5.0f MB %10.3fM %6.3f Hz\n",
	 (total_render_time/frame_count)*1000.0,
	 (total_read_time/frame_count)*1000.0,
	 (total_write_time/frame_count)*1000.0,
	 (total_compress_time/frame_count)*1000.0,
	 (total_compare_time/frame_count)*1000.0,
	 (total_compose_time/frame_count)*1000.0,
	 total_mbytes_trans/frame_count,
	 (total_tri_rate/frame_count)/1000000.0,
	 total_frame_rate/frame_count));

    exit(0);
}

static void write_ppm(const char *filename,
		      const unsigned char *image,
		      int width, int height)
{
    FILE *fd;
    int x, y;
    const unsigned char *color;
    GLint color_format;

    icetGetIntegerv(ICET_COLOR_FORMAT, &color_format);

    fd = fopen(filename, "wb");

    fprintf(fd, "P6\n");
    fprintf(fd, "# %s generated by tilecomp\n", filename);
    fprintf(fd, "%d %d\n", width, height);
    fprintf(fd, "255\n");

    for (y = height-1; y >= 0; y--) {
	color = image + y*width*4;
	for (x = 0; x < width; x++) {
	    switch (color_format) {
	      case GL_RGBA:
		  fwrite(color, 1, 3, fd);
		  break;
#ifdef GL_BGRA_EXT
	      case GL_BGRA_EXT:
		  fwrite(color+2, 1, 1, fd);
		  fwrite(color+1, 1, 1, fd);
		  fwrite(color+0, 1, 1, fd);
		  break;
#endif
	      default:
		  fprintf(stderr, "Bad color format.\n");
		  return;
	    }
	    color += 4;
	}
    }

    fclose(fd);
}

#if FANCY_COLORS
/* Mostly from Foley and van Dam. */
static void HSV2RGB(float h, float s, float v,
		    float *r, float *g, float *b)
{
    float f, p, q, t;
    int i;

  /* Get h in range [0,360) */
    while (h >= 360.0) h -= 360.0;
    while (h < 0.0) h += 360.0;

    h /= 60.0;		/* Map h to range [0,6). */
    i = floor(h);	/* Integer part of h. */
    f = h - i;		/* Fractional part of h. */

    p = v*(1.0 - s);
    q = v*(1.0 - s*f);
    t = v*(1.0 - s*(1.0 - f));

    switch (i) {
      case 0: *r = v; *g = t; *b = p; break;
      case 1: *r = q; *g = v; *b = p; break;
      case 2: *r = p; *g = v; *b = t; break;
      case 3: *r = p; *g = q; *b = v; break;
      case 4: *r = t; *g = p; *b = v; break;
      case 5: *r = v; *g = p; *b = q; break;
    }
}
#endif /*FANCY_COLORS*/
