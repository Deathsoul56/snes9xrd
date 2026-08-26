/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifdef HAVE_LIBPNG
#include <png.h>
#endif

#include <ctime>
#include <sstream>
#include <iomanip>
#include <cerrno>
#include <cstring>

#include "snes9x.h"
#include "memmap.h"
#include "screenshot.h"

#ifdef HAVE_LIBPNG
static std::string s9x_png_last_error;

static void s9x_png_error_fn(png_structp png_ptr, png_const_charp msg)
{
	s9x_png_last_error = msg ? msg : "unknown libpng error";
	longjmp(png_jmpbuf(png_ptr), 1);
}

static void s9x_png_warning_fn(png_structp, png_const_charp) {}
#endif

bool8 S9xDoScreenshot (int width, int height)
{
	Settings.TakeScreenshot = FALSE;

#ifdef HAVE_LIBPNG
	FILE		*fp;
	png_structp	png_ptr;
	png_infop	info_ptr;
	png_color_8	sig_bit;
	int			imgwidth, imgheight;

	std::tm *current_time;
	std::time_t current_timet = time(nullptr);
	current_time = localtime(&current_timet);

	std::stringstream ss;
	ss << "-" << std::put_time(current_time, "%Y-%m-%d-%H-%M-%S");
	std::string fname = S9xGetFilename(ss.str() + ".png", SCREENSHOT_DIR);

	for (int i = 0; i < 1000; i++)
	{
		FILE *fp = fopen(fname.c_str(), "r");

		if (!fp)
			break;

		fclose(fp);
		fname = S9xGetFilename(ss.str() + "-" + std::to_string(i) + ".png", SCREENSHOT_DIR);
	}

	fp = fopen(fname.c_str(), "wb");
	if (!fp)
	{
		std::string msg = "Failed to take screenshot: " + fname + " (" + strerror(errno) + ")";
		S9xMessage(S9X_ERROR, 0, msg.c_str());
		return (FALSE);
	}

	s9x_png_last_error.clear();
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, s9x_png_error_fn, s9x_png_warning_fn);
	if (!png_ptr)
	{
		fclose(fp);
		remove(fname.c_str());
		S9xMessage(S9X_ERROR, 0, "Failed to take screenshot: png_create_write_struct failed.");
		return (FALSE);
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		fclose(fp);
		remove(fname.c_str());
		S9xMessage(S9X_ERROR, 0, "Failed to take screenshot: png_create_info_struct failed.");
		return (FALSE);
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fp);
		remove(fname.c_str());
		std::string msg = "Failed to take screenshot: " + s9x_png_last_error;
		S9xMessage(S9X_ERROR, 0, msg.c_str());
		return (FALSE);
	}

	imgwidth  = width;
	imgheight = height;

	if (Settings.StretchScreenshots == 1)
	{
		if (width > SNES_WIDTH && height <= SNES_HEIGHT_EXTENDED)
			imgheight = height << 1;
	}
	else if (Settings.StretchScreenshots == 2)
	{
		if (width  <= SNES_WIDTH)
			imgwidth  = width  << 1;
		if (height <= SNES_HEIGHT_EXTENDED)
			imgheight = height << 1;
	}

	png_init_io(png_ptr, fp);

	png_set_IHDR(png_ptr, info_ptr, imgwidth, imgheight, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	// Recent libpng validates png_set_shift()'s true_bits against png_ptr->bit_depth/color_type,
	// which are only populated once IHDR is actually written -- force that now so the check
	// below doesn't see stale (zeroed) values and reject a valid shift.
	png_write_info_before_PLTE(png_ptr, info_ptr);

	sig_bit.red   = 5;
	sig_bit.green = 5;
	sig_bit.blue  = 5;
	png_set_sBIT(png_ptr, info_ptr, &sig_bit);
	png_set_shift(png_ptr, &sig_bit);

	png_write_info(png_ptr, info_ptr);

	png_set_packing(png_ptr);

	png_byte	*row_pointer = new png_byte[png_get_rowbytes(png_ptr, info_ptr)];
	uint16		*screen = GFX.Screen;

	for (int y = 0; y < height; y++, screen += GFX.RealPPL)
	{
		png_byte	*rowpix = row_pointer;

		for (int x = 0; x < width; x++)
		{
			uint32	r, g, b;

			DECOMPOSE_PIXEL(screen[x], r, g, b);

			*(rowpix++) = r;
			*(rowpix++) = g;
			*(rowpix++) = b;

			if (imgwidth != width)
			{
				*(rowpix++) = r;
				*(rowpix++) = g;
				*(rowpix++) = b;
			}
		}

		png_write_row(png_ptr, row_pointer);
		if (imgheight != height)
			png_write_row(png_ptr, row_pointer);
	}

	delete [] row_pointer;

	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(fp);

	fprintf(stderr, "%s saved.\n", fname.c_str());

	std::string base = "Saved screenshot " + S9xBasename(fname);
	S9xMessage(S9X_INFO, S9X_SCREENSHOT_INFO, base.c_str());

	return (TRUE);
#else
	fprintf(stderr, "Screenshot support not available (libpng was not found at build time).\n");
	return (FALSE);
#endif
}
