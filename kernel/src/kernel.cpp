#include <limine.h>
#include <atomic.hpp>

class Terminal
{
public:
    Terminal() = default;

    ~Terminal() {}

    [[nodiscard("Can't discard X value of terminal")]]
    float get_x() noexcept
    {
        return cx;
    }

private:
    float cx;
    float cy;
};

Terminal g;

Atomic<int> atomic_counter(0);
Atomic<bool> test_complete(false);

extern "C" void kernel_main()
{
    atomic_counter.store(42);
    int v = atomic_counter.load();
    test_complete.store(true);

    while (true) {}
}