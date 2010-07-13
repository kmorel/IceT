/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2010 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 */

#include <IceTGL.h>

#include <IceTDevDiagnostics.h>
#include <IceTDevState.h>

#include <IceTDevGLImage.h>

#include <stdlib.h>
#include <string.h>

static IceTUByte *display_buffer = NULL;
static IceTSizeType display_buffer_size = 0;

static void inflateBuffer(IceTUByte *buffer,
                          IceTSizeType width, IceTSizeType height);

void icetGLDrawCallback(IceTGLDrawCallbackType func)
{
    icetStateSetPointer(ICET_GL_DRAW_FUNCTION, (IceTVoid *)func);
}

IceTImage icetGLDrawFrame(void)
{
    IceTImage image;
    IceTInt display_tile;
    IceTBoolean color_blending;
    GLint physical_viewport[4];
    IceTFloat background_color[4];
    IceTDouble projection_matrix[16];
    IceTDouble modelview_matrix[16];
    IceTVoid *value;
    IceTDrawCallbackType original_callback;
    IceTDouble buf_write_time;

    icetGetIntegerv(ICET_TILE_DISPLAYED, &display_tile);

    color_blending =
        (IceTBoolean)(   *(icetUnsafeStateGetInteger(ICET_COMPOSITE_MODE))
                      == ICET_COMPOSITE_MODE_BLEND);

  /* Update physical render size to actual OpenGL viewport. */
    glGetIntegerv(GL_VIEWPORT, physical_viewport);
    icetPhysicalRenderSize(physical_viewport[2], physical_viewport[3]);

  /* Set the background color to OpenGL's background color. */
    glGetFloatv(GL_COLOR_CLEAR_VALUE, background_color);

  /* Get the current matrices. */
    glGetDoublev(GL_PROJECTION_MATRIX, projection_matrix);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview_matrix);

  /* Check the GL callback. */
    icetGetPointerv(ICET_GL_DRAW_FUNCTION, &value);
    if (value == NULL) {
        icetRaiseError("GL Drawing function not set.  Call icetGLDrawCallback.",
                       ICET_INVALID_OPERATION);
        return icetImageNull();
    }

  /* Set up core callback to call the GL layer. */
    icetGetPointerv(ICET_DRAW_FUNCTION, &value);
    original_callback = (IceTDrawCallbackType)value;
    icetDrawCallback(icetGLDrawCallbackFunction);

  /* Hand control to the core layer to render and composite. */
    image = icetDrawFrame(projection_matrix,
                          modelview_matrix,
                          background_color);

  /* Restore core IceT callback. */
    icetDrawCallback(original_callback);

  /* Fix background color. */
    glClearColor(background_color[0], background_color[1],
                 background_color[2], background_color[3]);

  /* Paste final image back to buffer if enabled. */
    buf_write_time = icetWallTime();
    if (display_tile >= 0) {
        IceTEnum color_format = icetImageGetColorFormat(image);

        if (   (color_format != ICET_IMAGE_COLOR_NONE)
            && icetIsEnabled(ICET_GL_DISPLAY) ) {
            IceTUByte *colorBuffer;
            IceTInt readBuffer;
            IceTInt *tile_viewports;

            icetRaiseDebug("Displaying image.");

            icetGetIntegerv(ICET_GL_READ_BUFFER, &readBuffer);
            glDrawBuffer(readBuffer);

          /* Place raster position in lower left corner. */
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();
            glRasterPos2f(-1, -1);
            glPopMatrix();

          /* This could be made more efficient by natively handling all the
             image formats.  Don't forget to free memory later if necessary. */
            if (icetImageGetColorFormat(image) == ICET_IMAGE_COLOR_RGBA_UBYTE) {
                colorBuffer = icetImageGetColorUByte(image);
            } else {
                colorBuffer = malloc(4*icetImageGetNumPixels(image));
                icetImageCopyColorUByte(image, colorBuffer,
                                        ICET_IMAGE_COLOR_RGBA_UBYTE);
            }

            glPushAttrib(GL_TEXTURE_BIT | GL_COLOR_BUFFER_BIT);
            glDisable(GL_TEXTURE_1D);
            glDisable(GL_TEXTURE_2D);
#ifdef GL_TEXTURE_3D
            glDisable(GL_TEXTURE_3D);
#endif
            if (   color_blending
                && icetIsEnabled(ICET_GL_DISPLAY_COLORED_BACKGROUND)
                && !icetIsEnabled(ICET_CORRECT_COLORED_BACKGROUND) ) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_BLEND);
                glClear(GL_COLOR_BUFFER_BIT);
            } else {
                glDisable(GL_BLEND);
            }
            glClear(GL_DEPTH_BUFFER_BIT);
            tile_viewports = icetUnsafeStateGetInteger(ICET_TILE_VIEWPORTS);
            if (icetIsEnabled(ICET_GL_DISPLAY_INFLATE)) {
                inflateBuffer(colorBuffer,
                              tile_viewports[display_tile*4+2],
                              tile_viewports[display_tile*4+3]);
            } else {
                glDrawPixels(tile_viewports[display_tile*4+2],
                             tile_viewports[display_tile*4+3],
                             GL_RGBA, GL_UNSIGNED_BYTE, colorBuffer);
            }
            glPopAttrib();

          /* Delete the color buffer if we had to create our own. */
            if (icetImageGetColorFormat(image) != ICET_IMAGE_COLOR_RGBA_UBYTE) {
                free(colorBuffer);
            }
        }
    }

  /* Restore matrices. */
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(projection_matrix);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixd(modelview_matrix);

  /* Calculate display times. */
    buf_write_time = icetWallTime() - buf_write_time;
    icetStateSetDouble(ICET_BUFFER_WRITE_TIME, buf_write_time);

    return image;
}

static void inflateBuffer(IceTUByte *buffer,
                          IceTSizeType width, IceTSizeType height)
{
    IceTInt display_width, display_height;

    icetGetIntegerv(ICET_PHYSICAL_RENDER_WIDTH, &display_width);
    icetGetIntegerv(ICET_PHYSICAL_RENDER_HEIGHT, &display_height);

    if ((display_width <= width) && (display_height <= height)) {
      /* No need to inflate image. */
        glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    } else {
        IceTSizeType x, y;
        IceTSizeType x_div, y_div;
        IceTUByte *last_scanline;
        IceTInt target_width, target_height;
        int use_textures = icetIsEnabled(ICET_GL_DISPLAY_INFLATE_WITH_HARDWARE);

      /* If using hardware, resize to the nearest greater power of two.
         Otherwise, resize to screen size. */
        if (use_textures) {
            for (target_width=1; target_width < width; target_width <<= 1);
            for (target_height=1; target_height < height; target_height <<= 1);
            if (target_width*target_height >= display_width*display_height) {
              /* Sizing up to a power of two takes more data than just
                 sizing up to the screen size. */
                use_textures = 0;
                target_width = display_width;
                target_height = display_height;
            }
        } else {
            target_width = display_width;
            target_height = display_height;
        }

      /* Make sure buffer is big enough. */
        if (display_buffer_size < target_width*target_height) {
            free(display_buffer);
            display_buffer_size = target_width*target_height;
            display_buffer = malloc(4*sizeof(IceTUByte)*display_buffer_size);
        }

      /* This is how we scale the image with integer arithmetic.
       * If a/b = r = a div b + (a mod b)/b then:
       *        c/r = c/(a div b + (a mod b)/b) = c*b/(b*(a div b) + a mod b)
       * In our case a/b is target_width/width and target_height/height.
       * x_div and y_div are the denominators in the equation above.
       */
        x_div = width*(target_width/width) + target_width%width;
        y_div = height*(target_height/height) + target_height%height;
        last_scanline = NULL;
        for (y = 0; y < target_height; y++) {
            IceTUByte *src_scanline;
            IceTUByte *dest_scanline;

            src_scanline = buffer + 4*width*((y*height)/y_div);
            dest_scanline = display_buffer + 4*target_width*y;

            if (src_scanline == last_scanline) {
              /* Repeating last scanline.  Just copy memory. */
                memcpy(dest_scanline,
                       (const IceTUByte *)(dest_scanline - 4*target_width),
                       4*target_width);
                continue;
            }

            for (x = 0; x < target_width; x++) {
                ((IceTUInt *)dest_scanline)[x] =
                    ((IceTUInt *)src_scanline)[(x*width)/x_div];
            }

            last_scanline = src_scanline;
        }

        if (use_textures) {
            IceTInt icet_texture;
            GLuint gl_texture;

          /* Setup texture. */
            icetGetIntegerv(ICET_GL_INFLATE_TEXTURE, &icet_texture);
            gl_texture = icet_texture;

            if (gl_texture == 0) {
                glGenTextures(1, &gl_texture);
                icet_texture = gl_texture;
                icetStateSetInteger(ICET_GL_INFLATE_TEXTURE, icet_texture);
            }

            glBindTexture(GL_TEXTURE_2D, gl_texture);
            glEnable(GL_TEXTURE_2D);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, target_width, target_height,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, display_buffer);

          /* Setup geometry. */
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

          /* Draw texture. */
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glBegin(GL_QUADS);
              glTexCoord2f(0, 0);  glVertex2f(-1, -1);
              glTexCoord2f(1, 0);  glVertex2f( 1, -1);
              glTexCoord2f(1, 1);  glVertex2f( 1,  1);
              glTexCoord2f(0, 1);  glVertex2f(-1,  1);
            glEnd();

          /* Clean up. */
            glPopMatrix();
        } else {
            glDrawPixels(target_width, target_height, GL_RGBA,
                         GL_UNSIGNED_BYTE, display_buffer);
        }
    }
}
