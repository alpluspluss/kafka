/* this file is a part of Kafka kernel which is under MIT license; see LICENSE for more info */

void __cxa_finalize(const void* d);

extern void* __dso_handle;

__attribute__((__destructor__, __used__))
static void cleanup(void) 
{
    __cxa_finalize(&__dso_handle);
}