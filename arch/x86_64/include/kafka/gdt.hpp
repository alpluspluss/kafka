/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <stdint.h>

namespace kfk
{
    struct GDTEntry
    {
        uint16_t limit_low;
		uint16_t base_low;
		uint8_t base_middle;
		uint8_t access;
		uint8_t limit_high: 4;
		uint8_t flags: 4;
		uint8_t base_high;
    } __attribute__((packed));

    struct GDTDescriptor
	{
		uint16_t size;
		uint64_t offset;
	} __attribute__((packed));
}