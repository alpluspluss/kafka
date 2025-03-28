/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

#pragma once

namespace kfk
{
    struct x86_64 {};

    struct aarch64 {};

#if defined(__x86_64__)
    using current_arch = x86_64;
#else
    using current_arch = aarch64;
#endif
}
