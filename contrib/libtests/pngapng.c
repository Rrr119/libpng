/* pngapng.c
 *
 * Roundtrip test for APNG frame data and fcTL metadata.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_CONFIG_H) && !defined(PNG_NO_CONFIG_H)
#  include <config.h>
#endif

#ifdef PNG_FREESTANDING_TESTS
#  include <png.h>
#else
#  include "../../png.h"
#endif

#if defined(PNG_APNG_SUPPORTED) && defined(PNG_READ_APNG_SUPPORTED) && \
    defined(PNG_WRITE_APNG_SUPPORTED) && \
    defined(PNG_SEQUENTIAL_READ_SUPPORTED) && defined(PNG_STDIO_SUPPORTED)

#define APNG_WIDTH  3
#define APNG_HEIGHT 2
#define APNG_FRAMES 3
#define APNG_ROWBYTES (APNG_WIDTH * 3)

typedef struct
{
   png_uint_32 width;
   png_uint_32 height;
   png_uint_32 x_offset;
   png_uint_32 y_offset;
   png_uint_16 delay_num;
   png_uint_16 delay_den;
   png_byte dispose_op;
   png_byte blend_op;
   png_byte pixels[APNG_HEIGHT][APNG_ROWBYTES];
} apng_frame;

static void
init_frames(apng_frame *frames)
{
   static const png_byte frame1[APNG_HEIGHT][APNG_ROWBYTES] =
   {
      { 255,   0,   0,   0, 255,   0,   0,   0, 255 },
      {   0, 255, 255, 255,   0, 255, 255, 255,   0 }
   };
   static const png_byte frame2[APNG_HEIGHT][APNG_ROWBYTES] =
   {
      {  16,  32,  48,  64,  80,  96,   0,   0,   0 },
      {  96,  80,  64,  48,  32,  16,   0,   0,   0 }
   };
   static const png_byte frame3[APNG_HEIGHT][APNG_ROWBYTES] =
   {
      {  10,  20,  30,  40,  50,  60,  70,  80,  90 },
      {  90,  80,  70,  60,  50,  40,  30,  20,  10 }
   };

   memset(frames, 0, sizeof *frames * APNG_FRAMES);

   frames[0].width = APNG_WIDTH;
   frames[0].height = APNG_HEIGHT;
   frames[0].delay_num = 1;
   frames[0].delay_den = 100;
   frames[0].dispose_op = PNG_fcTL_DISPOSE_OP_NONE;
   frames[0].blend_op = PNG_fcTL_BLEND_OP_SOURCE;
   memcpy(frames[0].pixels, frame1, sizeof frame1);

   frames[1].width = 2;
   frames[1].height = APNG_HEIGHT;
   frames[1].x_offset = 1;
   frames[1].delay_num = 7;
   frames[1].delay_den = 0;
   frames[1].dispose_op = PNG_fcTL_DISPOSE_OP_BACKGROUND;
   frames[1].blend_op = PNG_fcTL_BLEND_OP_OVER;
   memcpy(frames[1].pixels, frame2, sizeof frame2);

   frames[2].width = APNG_WIDTH;
   frames[2].height = APNG_HEIGHT;
   frames[2].delay_num = 3;
   frames[2].delay_den = 10;
   frames[2].dispose_op = PNG_fcTL_DISPOSE_OP_PREVIOUS;
   frames[2].blend_op = PNG_fcTL_BLEND_OP_SOURCE;
   memcpy(frames[2].pixels, frame3, sizeof frame3);
}

static int
write_apng(FILE *fp, const apng_frame *frames)
{
   png_structp png_ptr;
   png_infop info_ptr;
   png_uint_32 frame;
   int result = 1;

   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
       NULL, NULL, NULL);
   if (png_ptr == NULL)
   {
      fprintf(stderr, "pngapng: png_create_write_struct failed\n");
      return 1;
   }

   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL)
   {
      fprintf(stderr, "pngapng: png_create_info_struct failed\n");
      png_destroy_write_struct(&png_ptr, NULL);
      return 1;
   }

   if (setjmp(png_jmpbuf(png_ptr)))
   {
      fprintf(stderr, "pngapng: libpng error while writing\n");
      goto done;
   }

   png_init_io(png_ptr, fp);
   png_set_IHDR(png_ptr, info_ptr, APNG_WIDTH, APNG_HEIGHT, 8,
       PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
       PNG_FILTER_TYPE_BASE);
   png_set_acTL(png_ptr, info_ptr, APNG_FRAMES, 2);

   /* Force a filter that depends on the previous row.  The first row of each
    * APNG frame must start from an all-zero previous row.
    */
   png_set_filter(png_ptr, PNG_FILTER_TYPE_BASE, PNG_FILTER_UP);
   png_write_info(png_ptr, info_ptr);

   for (frame = 0; frame < APNG_FRAMES; frame++)
   {
      png_bytep rows[APNG_HEIGHT];
      png_uint_32 row;

      for (row = 0; row < frames[frame].height; row++)
         rows[row] = (png_bytep)frames[frame].pixels[row];

      png_write_frame_head(png_ptr, info_ptr, rows,
          frames[frame].width, frames[frame].height,
          frames[frame].x_offset, frames[frame].y_offset,
          frames[frame].delay_num, frames[frame].delay_den,
          frames[frame].dispose_op, frames[frame].blend_op);

      for (row = 0; row < frames[frame].height; row++)
         png_write_row(png_ptr, rows[row]);

      png_write_frame_tail(png_ptr, info_ptr);
   }

   png_write_end(png_ptr, NULL);
   result = 0;

done:
   png_destroy_write_struct(&png_ptr, &info_ptr);
   return result;
}

static int
check_frame_metadata(png_structp png_ptr, png_infop info_ptr,
                     const apng_frame *expected, png_uint_32 frame)
{
   png_uint_32 width;
   png_uint_32 height;
   png_uint_32 x_offset;
   png_uint_32 y_offset;
   png_uint_16 delay_num;
   png_uint_16 delay_den;
   png_byte dispose_op;
   png_byte blend_op;

   if (!png_get_valid(png_ptr, info_ptr, PNG_INFO_fcTL) ||
       !png_get_next_frame_fcTL(png_ptr, info_ptr,
          &width, &height, &x_offset, &y_offset,
          &delay_num, &delay_den, &dispose_op, &blend_op))
   {
      fprintf(stderr, "pngapng: missing fcTL for frame %lu\n",
          (unsigned long)frame);
      return 1;
   }

   if (width != expected->width || height != expected->height ||
       x_offset != expected->x_offset || y_offset != expected->y_offset ||
       delay_num != expected->delay_num ||
       delay_den != expected->delay_den ||
       dispose_op != expected->dispose_op ||
       blend_op != expected->blend_op)
   {
      fprintf(stderr, "pngapng: fcTL metadata mismatch for frame %lu\n",
          (unsigned long)frame);
      return 1;
   }

   return 0;
}

static int
read_apng(FILE *fp, const apng_frame *expected)
{
   png_structp png_ptr;
   png_infop info_ptr;
   png_uint_32 num_frames;
   png_uint_32 num_plays;
   png_uint_32 frame;
   int result = 1;

   png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
       NULL, NULL, NULL);
   if (png_ptr == NULL)
   {
      fprintf(stderr, "pngapng: png_create_read_struct failed\n");
      return 1;
   }

   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL)
   {
      fprintf(stderr, "pngapng: png_create_info_struct failed\n");
      png_destroy_read_struct(&png_ptr, NULL, NULL);
      return 1;
   }

   if (setjmp(png_jmpbuf(png_ptr)))
   {
      fprintf(stderr, "pngapng: libpng error while reading\n");
      goto done;
   }

   png_init_io(png_ptr, fp);
   png_read_info(png_ptr, info_ptr);

   if (!png_get_acTL(png_ptr, info_ptr, &num_frames, &num_plays) ||
       num_frames != APNG_FRAMES || num_plays != 2 ||
       png_get_num_frames(png_ptr, info_ptr) != APNG_FRAMES ||
       png_get_num_plays(png_ptr, info_ptr) != 2 ||
       png_get_first_frame_is_hidden(png_ptr, info_ptr) != 0)
   {
      fprintf(stderr, "pngapng: acTL metadata mismatch\n");
      goto done;
   }

   for (frame = 0; frame < APNG_FRAMES; frame++)
   {
      png_byte actual[APNG_HEIGHT][APNG_ROWBYTES];
      png_bytep rows[APNG_HEIGHT];
      png_uint_32 row;
      size_t rowbytes;

      png_read_frame_head(png_ptr, info_ptr);

      if (check_frame_metadata(png_ptr, info_ptr, expected + frame, frame))
         goto done;

      rowbytes = (size_t)expected[frame].width * 3;
      memset(actual, 0, sizeof actual);

      for (row = 0; row < expected[frame].height; row++)
         rows[row] = actual[row];

      png_read_image(png_ptr, rows);

      for (row = 0; row < expected[frame].height; row++)
      {
         if (memcmp(actual[row], expected[frame].pixels[row], rowbytes) != 0)
         {
            fprintf(stderr, "pngapng: pixel mismatch in frame %lu row %lu\n",
                (unsigned long)frame, (unsigned long)row);
            goto done;
         }
      }
   }

   png_read_end(png_ptr, NULL);
   result = 0;

done:
   png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
   return result;
}

#endif /* APNG support */

int
main(void)
{
#if defined(PNG_APNG_SUPPORTED) && defined(PNG_READ_APNG_SUPPORTED) && \
    defined(PNG_WRITE_APNG_SUPPORTED) && \
    defined(PNG_SEQUENTIAL_READ_SUPPORTED) && defined(PNG_STDIO_SUPPORTED)
   apng_frame frames[APNG_FRAMES];
   FILE *fp;
   int result;

   init_frames(frames);

   fp = tmpfile();
   if (fp == NULL)
   {
      fprintf(stderr, "pngapng: tmpfile failed\n");
      return 1;
   }

   result = write_apng(fp, frames);
   if (result == 0)
   {
      if (fseek(fp, 0, SEEK_SET) != 0)
      {
         fprintf(stderr, "pngapng: fseek failed\n");
         result = 1;
      }
      else
         result = read_apng(fp, frames);
   }

   fclose(fp);

   if (result == 0)
      printf("pngapng: multi-frame roundtrip passed\n");

   return result;
#else
   printf("pngapng: APNG support is disabled\n");
   return 0;
#endif
}
