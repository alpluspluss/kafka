/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

#include <stdint.h>

namespace kfk
{
	/* TSS */
	struct TSS
	{
		uint32_t reserved0;
		uint64_t rsp[3]; /* rsp0-3 */
		uint64_t reserved1;
		uint64_t ist[7]; /* ist1-7 */
		uint64_t reserved2;
		uint16_t reserved3;
		uint16_t iopb;
	} __attribute__((packed));
}