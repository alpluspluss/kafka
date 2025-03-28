/* this file is a spart of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <limine.h>
#include <stdint.h>

namespace kfk
{
    namespace Colors
	{
		constexpr uint32_t BLACK = 0x000000;
		constexpr uint32_t WHITE = 0xFFFFFF;
		constexpr uint32_t RED = 0xFF0000;
		constexpr uint32_t GREEN = 0x00FF00;
		constexpr uint32_t BLUE = 0x0000FF;
		constexpr uint32_t YELLOW = 0xFFFF00;
		constexpr uint32_t CYAN = 0x00FFFF;
		constexpr uint32_t MAGENTA = 0xFF00FF;
		constexpr uint32_t GRAY = 0x808080;
		constexpr uint32_t DARK_BLUE = 0x000080;
	}

    class Fb
    {
    public:
        struct FBInfo
        {
            uint64_t width;
            uint64_t height;
            uint64_t bpp; /* byte per pixel */
            uint64_t pitch;
        };

        enum class TextAlignment
        {
            LEFT,
            CENTER,
            RIGHT
        };

        /* init the fb renderer */
		static void init(limine_framebuffer* fb) noexcept;

		/* get information about the framebuffer */
		static FBInfo fbinfo() noexcept;

		/* draw a single pixel */
		static void draw_pixel(int x, int y, uint32_t color) noexcept;

		/* fill a recangle on the framebuffer */
		static void fill_rect(int x, int y, uint32_t width, uint32_t height, uint32_t color) noexcept;

		/* draw a horizontal line */
		static void draw_hline(int x1, int x2, int y, uint32_t color) noexcept;

		/* draw a vertical line */
		static void draw_vline(int x, int y1, int y2, uint32_t color) noexcept;

		/* draw an outline rectangle */
		static void draw_rect(int x, int y, uint32_t width, uint32_t height, uint32_t color) noexcept;

		/* clear the entire fb with a color */
		static void clear_screen(uint32_t color) noexcept;

		/* draw a single character on the fb */
		static int draw_char(char c, int x, int y, uint32_t color) noexcept;

		/* draw a text string on the framebuffer */
		static int draw_text(const char* text, int x, int y, uint32_t color, TextAlignment alignment = TextAlignment::LEFT) noexcept;

		/* text to measure */
		static uint32_t text_width(const char* text) noexcept;

		static uint32_t width() noexcept; /* get fb width */

		static uint32_t height() noexcept; /* get fb height */

		static uint32_t font_height() noexcept; /* get font height */
    };

	using fb = Fb;
}