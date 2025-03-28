/* this file is a spart of Kafka kernel which is under MIT license; see LICENSE for more info */

#include <kafka/fb.hpp>
#include <kafka/res/fonts.h>

namespace kfk
{
    namespace g
    {
        const limine_framebuffer* fb = nullptr;
		auto init = false;
    }

    void Fb::init(limine_framebuffer *fb) noexcept
    {
        if (!fb || !fb->address)
            return;

        g::fb = fb;
        g::init = true;
    }

    Fb::FBInfo Fb::fbinfo() noexcept
    {
        if (!g::init)
			return { 0, 0, 0, 0 };

		return {
			g::fb->width,
			g::fb->height,
			g::fb->bpp,
			g::fb->pitch
		};
    }

    void Fb::draw_pixel(int x, int y, uint32_t color) noexcept
    {
        if (!g::init)
			return;

		if (x < 0 || y < 0 ||
			x >= static_cast<int>(g::fb->width) ||
			y >= static_cast<int>(g::fb->height))
			return;

		const uint32_t bpp = g::fb->bpp / 8;
		if (bpp == 0) /* to not get a page fault */
			return;

		const uint32_t ppr = g::fb->pitch / bpp; /* pixels per row */
		uint32_t* p = static_cast<uint32_t*>(g::fb->address) + y * ppr + x;
		*p = color;
    }

    void Fb::draw_hline(int x1, int x2, int y, uint32_t color) noexcept
    {
        if (!g::fb)
			return;

		if (x1 > x2)
		{
			/* swap */
			const auto t = x1;
			x1 = x2;
			x2 = t;
		}

		/* clip against screen bounds */
		if (y < 0 || y >= static_cast<int>(g::fb->height))
			return;

		if (x1 < 0)
			x1 = 0;
		if (x2 >= static_cast<int>(g::fb->width))
			x2 = g::fb->width - 1;

		if (x1 > x2)
			return;

		const uint32_t bpp = g::fb->bpp / 8;
		if (bpp == 0)
			return;

		const uint32_t ppr = g::fb->pitch / bpp;
		uint32_t* pixel = static_cast<uint32_t*>(g::fb->address) + y * ppr + x1;
		for (int x = x1; x <= x2; x++)
			*pixel++ = color;
    }

    void Fb::draw_vline(int x, int y1, int y2, uint32_t color) noexcept
    {
        if (!g::init)
			return;

		if (y1 > y2)
		{
			const int t = y1;
			y1 = y2;
			y2 = t;
		}

		if (x < 0 || x >= static_cast<int>(g::fb->width))
			return;

		if (y1 < 0)
			y1 = 0;
		if (y2 >= static_cast<int>(g::fb->height))
			y2 = g::fb->height - 1;

		if (y1 > y2)
			return;

		const uint32_t bpp = g::fb->bpp / 8;
		if (bpp == 0)
			return;

		const uint32_t ppr = g::fb->pitch / bpp;
		for (int y = y1; y <= y2; y++)
		{
			uint32_t* pixel = static_cast<uint32_t*>(g::fb->address) + y * ppr + x;
			*pixel = color;
		}
    }

    void Fb::draw_rect(int x, int y, uint32_t width, uint32_t height, uint32_t color) noexcept
    {
        if (!g::init)
			return;

		draw_hline(x, x + width - 1, y, color); /* top */
		draw_hline(x, x + width - 1, y + height - 1, color); /* bottom */
		draw_vline(x, y, y + height - 1, color); /* left */
		draw_vline(x + width - 1, y, y + height - 1, color); /* right */
    }

    void Fb::fill_rect(int x, int y, uint32_t width, uint32_t height, uint32_t color) noexcept
    {
        if (!g::fb)
			return;

		/* clip against screen bounds */
		if (x < 0)
		{
			width += x;
			x = 0;
		}
		if (y < 0)
		{
			height += y;
			y = 0;
		}

		if (x >= static_cast<int>(g::fb->width) || y >= static_cast<int>(g::fb->height))
			return;

		if (x + width > g::fb->width)
			width = g::fb->width - x;
		if (y + height > g::fb->height)
			height = g::fb->height - y;

		if (width == 0 || height == 0)
			return;

		const uint32_t bpp = g::fb->bpp / 8;
		if (bpp == 0)
			return;

		const uint32_t ppr = g::fb->pitch / bpp;
		uint32_t* base = static_cast<uint32_t*>(g::fb->address) + y * ppr + x;

		for (uint32_t row = 0; row < height; ++row)
		{
			uint32_t* p = base + row * ppr;
			if (width < 8)
			{
				for (uint32_t col = 0; col < width; ++col)
					*p++ = color;
			}
			else
			{
				uint32_t col = 0;
				for (; col + 7 < width; col += 8)
				{
					*p++ = color; *p++ = color; *p++ = color; *p++ = color;
					*p++ = color; *p++ = color; *p++ = color; *p++ = color;
				}
				for (; col < width; ++col)
					*p++ = color;
			}
		}
    }

    void Fb::clear_screen(uint32_t color) noexcept
    {
        if (!g::fb)
			return;

		const uint32_t bpp = g::fb->bpp / 8;
		if (bpp == 0)
			return;

		const uint32_t ppr = g::fb->pitch / bpp;
		auto* fb_start = static_cast<uint32_t*>(g::fb->address);
		const uint32_t total_pixels = g::fb->height * ppr;
		uint32_t i = 0;
		for (; i + 15 < total_pixels; i += 16)
		{
			fb_start[i+0] = color; fb_start[i+1] = color;
			fb_start[i+2] = color; fb_start[i+3] = color;
			fb_start[i+4] = color; fb_start[i+5] = color;
			fb_start[i+6] = color; fb_start[i+7] = color;
			fb_start[i+8] = color; fb_start[i+9] = color;
			fb_start[i+10] = color; fb_start[i+11] = color;
			fb_start[i+12] = color; fb_start[i+13] = color;
			fb_start[i+14] = color; fb_start[i+15] = color;
		}

		/* remainders */
		for (; i < total_pixels; ++i)
			fb_start[i] = color;
    }

    int Fb::draw_char(char c, int x, int y, uint32_t color) noexcept
    {
        if (!g::init)
			return FONT_CHAR_WIDTH / 2;

		const int char_index = static_cast<uint8_t>(c) - FONT_FIRST_CHAR;
		if (char_index < 0 || char_index >= FONT_CHAR_COUNT)
			return FONT_CHAR_WIDTH / 2;

		/* if the character is completely off-screen */
		if (x + FONT_CHAR_WIDTH/2 <= 0 || y + FONT_CHAR_HEIGHT <= 0 ||
			x >= static_cast<int>(g::fb->width) || y >= static_cast<int>(g::fb->height))
			return FONT_CHAR_WIDTH / 2;

		const uint8_t* bmp = BITMAP_FONT[char_index];
		const uint32_t bpp = g::fb->bpp / 8;
		const uint32_t ppr = g::fb->pitch / bpp;

		uint32_t* base_addr = static_cast<uint32_t*>(g::fb->address) + y * ppr + x;
		for (auto row = 0; row < FONT_CHAR_HEIGHT; row++)
		{
			if (y + row >= static_cast<int>(g::fb->height))
				break;
			if (y + row < 0)
				continue;

			uint32_t* row_addr = base_addr + row * ppr;
			for (auto bindex = 0; bindex < FONT_BYTES_PER_ROW; ++bindex)
			{
				const uint8_t byte = bmp[row * FONT_BYTES_PER_ROW + bindex];
				if (byte == 0) /* skip empty bytes */
					continue;

				for (auto col = 0; col < 8; col++)
				{
					if (bindex * 8 + col >= FONT_CHAR_WIDTH / 2)
						break;

					if (const int px = x + (bindex * 8 + col);
						px < 0 || px >= static_cast<int>(g::fb->width))
						continue;

					if (byte & (1 << (7 - col)))
						row_addr[bindex * 8 + col] = color;
				}
			}
		}
        return FONT_CHAR_WIDTH / 2;
    }
    
    int Fb::draw_text(const char* text, int x, int y, uint32_t color, TextAlignment alignment) noexcept
    {
        if (!g::init || !text)
			return 0;

		if (alignment != TextAlignment::LEFT)
		{
			const auto tw = text_width(text);
			if (alignment == TextAlignment::CENTER)
				x -= tw / 2;
			else if (alignment == TextAlignment::RIGHT)
				x -= tw;
		}

		auto cx = x; /* x-pos cursor */
		while (*text)
		{
			cx += draw_char(*text, cx, y, color);
			text++;
		}
		return cx - x;
    }

    uint32_t Fb::text_width(const char *text) noexcept
    {
        if (!text)
			return 0;

		uint32_t width = 0;
		while (*text)
		{
			width += FONT_CHAR_WIDTH / 2;
			text++;
		}
		return width;
    }

    uint32_t Fb::width() noexcept
	{
		return g::init ? g::fb->width : 0;
	}

	uint32_t Fb::height() noexcept
	{
		return g::init ? g::fb->height : 0;
	}

	uint32_t Fb::font_height() noexcept
	{
		return FONT_CHAR_HEIGHT;
	}
}
