#pragma once

#include <stddef.h>
#include <stdarg.h>
#include <kafka/fb.hpp>

namespace kfk
{
    constexpr size_t MAX_STRLEN = 256;

    /* manipulators; like std::hex of sort */
	enum FormatMode
	{
		DEC,
		HEX,
		BIN
	};

    namespace 
    {
        struct hex_t {};
        struct dec_t {};
        struct bin_t {};

        int cursor_x = 0;
        auto cursor_y = 0;
        uint32_t text_color = Colors::WHITE;
        auto alignment = Fb::TextAlignment::LEFT;
        FormatMode format_mode = DEC;
        auto uppercase_hex = true;
    }

    inline void set_cursor(const int x, const int y) noexcept
    {
        cursor_x = x;
        cursor_y = y;
    }

    inline void get_cursor(int *x, int *y) noexcept
    {
        if (x)
            *x = cursor_x;
        if (y)
            *y = cursor_y;
    }

    inline void set_color(uint32_t color) noexcept
    {
        text_color = color;
    }

    inline void set_alignment(const Fb::TextAlignment align) noexcept
    {
        alignment = align;
    }

    inline void set_format(const FormatMode mode) noexcept
    {
        format_mode = mode;
    }

    inline void set_uppercase(const bool enable) noexcept
    {
        uppercase_hex = enable;
    }

    inline void int_to_str(const int value, char *buffer, const size_t buffer_size, const FormatMode mode) noexcept
    {
        if (!buffer || buffer_size < 2)
            return;

        int base;
        auto prefix = "";
        switch (mode)
        {
            case HEX:
                base = 16;
                prefix = "0x";
                break;

            case BIN:
                base = 2;
                prefix = "0b";
                break;

            case DEC:
            default:
                base = 10;
                prefix = "";
                break;
        }

        bool negative = (value < 0) && (mode == DEC);
        uint32_t abs_value = negative ? -value : value;
        size_t prefix_len = 0;
        if (mode != DEC)
        {
            while (prefix[prefix_len] && prefix_len < buffer_size - 1)
            {
                buffer[prefix_len] = prefix[prefix_len];
                prefix_len++;
            }
        }

        uint32_t temp[33] = {};
        auto pos = 0;

        do
        {
            if (const uint8_t digit = abs_value % base;
                digit < 10)
                temp[pos++] = static_cast<uint8_t>('0') + digit;
            else
                temp[pos++] = (uppercase_hex ? 'A' : 'a') + (digit - 10);
            abs_value /= base;
        }
        while (abs_value > 0 && pos < 32);

        if (negative)
        {
            buffer[0] = '-';
            prefix_len = 1;
        }

        for (auto i = 0; i < pos && (prefix_len + i) < buffer_size - 1; i++)
            buffer[prefix_len + i] = temp[pos - i - 1];

        buffer[prefix_len + (pos < buffer_size - prefix_len - 1 ? pos : buffer_size - prefix_len - 1)] = '\0';
    }

    inline void uint_to_str(unsigned int value, char *buffer, size_t buffer_size, FormatMode mode) noexcept
    {
        if (!buffer || buffer_size < 2)
            return;

        int base;
        auto prefix = "";
        switch (mode)
        {
            case HEX:
                base = 16;
                prefix = "0x";
                break;

            case BIN:
                base = 2;
                prefix = "0b";
                break;

            case DEC:
            default:
                base = 10;
                prefix = "";
                break;
        }

        size_t prefix_len = 0;
        if (mode != DEC)
        {
            while (prefix[prefix_len] && prefix_len < buffer_size - 1)
            {
                buffer[prefix_len] = prefix[prefix_len];
                prefix_len++;
            }
        }

        /* conv2str */
        char temp[33] = {};
        auto pos = 0;
        do
        {
            if (unsigned int digit = value % base;
                digit < 10)
                temp[pos++] = '0' + digit;
            else
                temp[pos++] = (uppercase_hex ? 'A' : 'a') + (digit - 10);
            value /= base;
        }
        while (value > 0 && pos < 32);
        for (auto i = 0; i < pos && (prefix_len + i) < buffer_size - 1; i++)
            buffer[prefix_len + i] = temp[pos - i - 1];

        buffer[prefix_len + (pos < buffer_size - prefix_len - 1 ? pos : buffer_size - prefix_len - 1)] = '\0';
    }

    inline void print(const char *str) noexcept
    {
        if (!str)
            return;

        cursor_x += Fb::draw_text(
            str,
            cursor_x,
            cursor_y,
            text_color,
            alignment
        );
    }

    inline void print(char c) noexcept
    {
        const char str[2] = { c, '\0' };
        print(str);
    }

    inline void print(hex_t) noexcept
    {
        format_mode = HEX;
    }

    inline void print(dec_t) noexcept
    {
        format_mode = DEC;
    }

    inline void print(bin_t) noexcept
    {
        format_mode = BIN;
    }

    inline void println()
    {
        cursor_x = 20;
        cursor_y += Fb::font_height();

        /* wrap up to the top; TODO: scrolling */
        if (cursor_y >= Fb::height() - Fb::font_height())
            cursor_y = 0;
    }

    inline int vprintf_len(const char *format, va_list args, char *buffer, size_t buffer_size) noexcept
    {
        if (!format || !buffer || buffer_size == 0)
            return 0;

        size_t pos = 0;

        while (*format && pos < buffer_size - 1)
        {
            if (*format == '%')
            {
                format++; /* skip % */

                auto width = 0;
                auto zero_pad = false;
                auto left_align = false;
                if (*format == '-')
                {
                    left_align = true;
                    format++;
                }

                if (*format == '0')
                {
                    zero_pad = true;
                    format++;
                }

                while (*format >= '0' && *format <= '9')
                {
                    width = width * 10 + (*format - '0');
                    format++;
                }

                switch (*format)
                {
                    case 'd':
                    {
                        int value = va_arg(args, int);
                        char num_buffer[33];
                        int_to_str(value, num_buffer, sizeof(num_buffer), DEC);

                        auto len = 0;
                        while (num_buffer[len])
                            len++;

                        if (width > len && !left_align)
                        {
                            int pad_count = width - len;
                            for (auto i = 0; i < pad_count && pos < buffer_size - 1; i++)
                                buffer[pos++] = zero_pad ? '0' : ' ';
                        }

                        for (auto i = 0; num_buffer[i] && pos < buffer_size - 1; i++)
                            buffer[pos++] = num_buffer[i];

                        if (width > len && left_align)
                        {
                            int pad_count = width - len;
                            for (auto i = 0; i < pad_count && pos < buffer_size - 1; i++)
                                buffer[pos++] = ' ';
                        }
                        break;
                    }

                    case 'u':
                    {
                        unsigned int value = va_arg(args, unsigned int);
                        char num_buffer[33];
                        uint_to_str(value, num_buffer, sizeof(num_buffer), DEC);

                        int len = 0;
                        while (num_buffer[len])
                            len++;

                        if (width > len && !left_align)
                        {
                            int pad_count = width - len;
                            for (auto i = 0; i < pad_count && pos < buffer_size - 1; i++)
                                buffer[pos++] = zero_pad ? '0' : ' ';
                        }

                        for (auto i = 0; num_buffer[i] && pos < buffer_size - 1; i++)
                            buffer[pos++] = num_buffer[i];

                        if (width > len && left_align)
                        {
                            int pad_count = width - len;
                            for (auto i = 0; i < pad_count && pos < buffer_size - 1; i++)
                                buffer[pos++] = ' ';
                        }
                        break;
                    }

                    case 'x':
                    case 'X':
                    {
                        unsigned int value = va_arg(args, unsigned int);
                        bool old_case = uppercase_hex;
                        uppercase_hex = (*format == 'X');
                        char num_buffer[33];
                        uint_to_str(value, num_buffer, sizeof(num_buffer), HEX);
                        uppercase_hex = old_case;

                        auto len = 0;
                        while (num_buffer[len])
                            len++;

                        if (width > len && !left_align)
                        {
                            int pad_count = width - len;
                            for (int i = 0; i < pad_count && pos < buffer_size - 1; i++)
                                buffer[pos++] = zero_pad ? '0' : ' ';
                        }

                        for (auto i = 0; num_buffer[i] && pos < buffer_size - 1; i++)
                            buffer[pos++] = num_buffer[i];

                        if (width > len && left_align)
                        {
                            int pad_count = width - len;
                            for (int i = 0; i < pad_count && pos < buffer_size - 1; i++)
                                buffer[pos++] = ' ';
                        }
                        break;
                    }

                    case 'b':
                    {
                        unsigned int value = va_arg(args, unsigned int);
                        char num_buffer[35];
                        uint_to_str(value, num_buffer, sizeof(num_buffer), BIN);

                        auto len = 0;
                        while (num_buffer[len])
                            len++;

                        if (width > len && !left_align)
                        {
                            int pad_count = width - len;
                            for (auto i = 0; i < pad_count && pos < buffer_size - 1; i++)
                                buffer[pos++] = zero_pad ? '0' : ' ';
                        }

                        for (auto i = 0; num_buffer[i] && pos < buffer_size - 1; i++)
                            buffer[pos++] = num_buffer[i];

                        if (width > len && left_align)
                        {
                            int pad_count = width - len;
                            for (int i = 0; i < pad_count && pos < buffer_size - 1; i++)
                                buffer[pos++] = ' ';
                        }
                        break;
                    }

                    case 'c':
                    {
                        auto c = static_cast<char>(va_arg(args, int));
                        if (width > 1 && !left_align)
                        {
                            int pad_count = width - 1;
                            for (auto i = 0; i < pad_count && pos < buffer_size - 1; i++)
                                buffer[pos++] = ' ';
                        }

                        if (pos < buffer_size - 1)
                            buffer[pos++] = c;

                        if (width > 1 && left_align)
                        {
                            int pad_count = width - 1;
                            for (auto i = 0; i < pad_count && pos < buffer_size - 1; i++)
                                buffer[pos++] = ' ';
                        }
                        break;
                    }

                    case 's':
                    {
                        const char *str = va_arg(args, const char*);
                        if (!str)
                            str = "(null)";

                        auto len = 0;
                        while (str[len])
                            len++;

                        if (width > len && !left_align)
                        {
                            int pad_count = width - len;
                            for (int i = 0; i < pad_count && pos < buffer_size - 1; i++)
                                buffer[pos++] = ' ';
                        }

                        for (auto i = 0; i < len && pos < buffer_size - 1; i++)
                            buffer[pos++] = str[i];

                        if (width > len && left_align)
                        {
                            int pad_count = width - len;
                            for (int i = 0; i < pad_count && pos < buffer_size - 1; i++)
                                buffer[pos++] = ' ';
                        }
                        break;
                    }

                    case 'p':
                    {
                        void* ptr = va_arg(args, void*);
                        if (pos + 1 < buffer_size - 1)
                        {
                            buffer[pos++] = '0';
                            buffer[pos++] = 'x';
                        }

                        auto ptr_val = reinterpret_cast<unsigned long>(ptr);
                        char ptr_buffer[20];
                        auto ptr_len = 0;
                        for (int i = (sizeof(void*) * 2) - 1; i >= 0; i--)
                        {
                            unsigned int digit = (ptr_val >> (i * 4)) & 0xF;
                            ptr_buffer[ptr_len++] = digit < 10 ? '0' + digit : 'A' + (digit - 10);
                        }

                        for (int i = 0; i < ptr_len && pos < buffer_size - 1; i++)
                            buffer[pos++] = ptr_buffer[i];
                        break;
                    }

                    case '%':
                        if (pos < buffer_size - 1)
                            buffer[pos++] = '%';
                        break;

                    default:
                        if (pos < buffer_size - 1)
                            buffer[pos++] = '%';
                        if (pos < buffer_size - 1)
                            buffer[pos++] = *format;
                        break;
                }

                format++;
            }
            else
            {
                buffer[pos++] = *format++;
            }
        }

        buffer[pos] = '\0';
        return pos;
    }

    inline int vprintf(const char *format, va_list args) noexcept
    {
        char buffer[MAX_STRLEN];
        const int len = vprintf_len(format, args, buffer, sizeof(buffer));
        print(buffer);
        return len;
    }

    inline int printf(const char *format, ...) noexcept
    {
        va_list args;
        va_start(args, format);
        const int result = vprintf(format, args);
        va_end(args);
        return result;
    }

    inline int println(const char *format, ...) noexcept
    {
        va_list args;
        va_start(args, format);
        const int result = vprintf(format, args);
        va_end(args);
        println();
        return result;
    }

    inline void clear() noexcept
    {
        Fb::clear_screen(Colors::BLACK);
        cursor_x = 20;
        cursor_y = 80;
    }
}