/*-------------------------------------------------------------------------
 *
 *          ASCII Art extension for PostgreSQL
 *
 * Copyright (c) 2015, PostgreSQL Global Development Group
 *
 * This software is released under the PostgreSQL Licence.
 *
 * Author: Alexander Korotkov <a.korotkov@postgrespro.ru>
 *
 * IDENTIFICATION
 *    pg_aa/pg_aa.c
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "c.h"
#include "fmgr.h"
#include "mb/pg_wchar.h"
#include "utils/builtins.h"
#include "utils/pg_locale.h"

#include <gd.h>
#include <aalib.h>
#include <caca.h>
#include <stdio.h>
#include <math.h>

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(aa_out);
Datum		aa_out(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(caca_out);
Datum		caca_out(PG_FUNCTION_ARGS);

static uint8
get_intensity(gdImagePtr tb, int x, int y)
{
	int pixel = gdImageGetTrueColorPixel(tb, x, y);
	unsigned char grayscaled;
	float red = (float) gdTrueColorGetRed(pixel) / 255.0f,
		  green = (float) gdTrueColorGetGreen(pixel) / 255.0f,
		  blue = (float) gdTrueColorGetBlue(pixel) / 255.0f;

	grayscaled = round((0.30f * red + 0.59f * green + 0.11f * blue) * 255.0f);
	return grayscaled;
}

Datum
aa_out(PG_FUNCTION_ARGS)
{
	bytea	   *img = PG_GETARG_BYTEA_PP(0);
	int			width = PG_GETARG_INT32(1),
				height;
	gdImagePtr	im,
				tb;
	struct aa_hardware_params params;
	aa_context  *context;
	int			i,
				j;
	text	   *result;
	char	   *s,
			   *d,
			   *old_locale;

	/* Switch locale to C to force pg_aa use only ASCII symbols */
	old_locale = pstrdup(setlocale(LC_CTYPE, NULL));
	setlocale(LC_CTYPE, "C");

	/* Load image from png */
	im = gdImageCreateFromPngPtr(VARSIZE_ANY_EXHDR(img), VARDATA_ANY(img));
	PG_FREE_IF_COPY(img, 0);
	if (!im)
	{
		setlocale(LC_CTYPE, old_locale);
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("error loading png")));
	}

	/* Assume that charater is twice height-elongated */
	height = width * gdImageSY(im) / gdImageSX(im) / 2;

	/* Resample image first, libaa accepts 2x2 virtual pixels per charater */
	tb = gdImageCreateTrueColor(width * 2, height * 2);
	if (!tb)
	{
		gdImageDestroy(im);
		setlocale(LC_CTYPE, old_locale);
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("error creating image")));
	}
	gdImageCopyResampled(tb, im, 0, 0, 0, 0,
						 gdImageSX(tb), gdImageSY(tb),
						 gdImageSX(im), gdImageSY(im));
	gdImageDestroy(im);

	/* Initialize libaa */
	memset(&params, 0, sizeof(params));
	params.width = width;
	params.height = height;
	params.supported = AA_NORMAL_MASK | AA_DIM_MASK | AA_BOLD_MASK;
	context = aa_init(&mem_d, &params, NULL);
	if (context == NULL)
	{
		gdImageDestroy(tb);
		setlocale(LC_CTYPE, old_locale);
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("cannot initialize libaa")));
	}

	/* Put grayscaled pixels to libaa context */
	for (i = 0; i < gdImageSX(tb); i++)
	{
		for (j = 0; j < gdImageSY(tb); j++)
			aa_putpixel(context, i, j, get_intensity(tb, i, j));
	}

	/* Render ASCII-art using libaa */
	aa_render(context, &aa_defrenderparams, 0, 0,
			aa_scrwidth(context), aa_scrheight(context));
	aa_flush(context);
	s = (char *) aa_text(context);

	/* Convert the result into newline-separated text datatype */
	result = (text *) palloc(VARHDRSZ +
							 (width + 1) * height * MAX_MULTIBYTE_CHAR_LEN);
	d = VARDATA(result);
	for (i = 0; i < height; i++)
	{
		if (i > 0)
			*d++ = '\n';
		for (j = 0; j < width; j++)
			*d++ = *s++;
	}
	SET_VARSIZE(result, d - (char *)result);

	/* Cleanup resources */
	gdImageDestroy(tb);
	aa_close(context);
	setlocale(LC_CTYPE, old_locale);
	pfree(old_locale);

	PG_RETURN_TEXT_P(result);
}

Datum
caca_out(PG_FUNCTION_ARGS)
{
	bytea		   *img = PG_GETARG_BYTEA_PP(0);
	int				width = PG_GETARG_INT32(1), height;
	gdImagePtr		im;
	caca_canvas_t  *cv;
	caca_dither_t  *dither;
	uint32_t	   *pixels;
	int				i,
					j,
					k;
	size_t			len;
	char		   *buffer;
	text		   *result;

	/* Load image from png */
	im = gdImageCreateFromPngPtr(VARSIZE_ANY_EXHDR(img), VARDATA_ANY(img));
	PG_FREE_IF_COPY(img, 0);
	if (!im)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("error loading png")));
	}

	/* Assume that charater is twice height-elongated */
	height = width * gdImageSY(im) / gdImageSX(im) / 2;

	/* Copy image to libcaca canvas */
	cv = caca_create_canvas(width, height);
	pixels = (uint32_t *) palloc(sizeof(uint32_t) * gdImageSX(im) * gdImageSY(im));
	k = 0;
	for (i = 0; i < gdImageSY(im); i++)
		for (j = 0; j < gdImageSX(im); j++)
			pixels[k++] = (uint32_t) gdImageTrueColorPixel(im, j, i);

	/* Render image using libcaca */
	dither = caca_create_dither(32, gdImageSX(im), gdImageSY(im), 4 * gdImageSX(im),
								0x00ff0000, 0x0000ff00, 0x000000ff, 0x0);
	caca_dither_bitmap(cv, 0, 0, caca_get_canvas_width(cv),
					   caca_get_canvas_height(cv), dither, pixels);
	caca_free_dither(dither);

	/* Convert output buffer to text datatype */
	buffer = (char *) caca_export_canvas_to_memory(cv, "utf8", &len);
	buffer[len - 1] = '\0';
	result = cstring_to_text(buffer);

	/* Cleanup resources */
	free(buffer);
	caca_free_canvas(cv);

	PG_RETURN_TEXT_P(result);
}
