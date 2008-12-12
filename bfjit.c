#ifndef __i386__
#error i386-only
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>

typedef struct buffer_t
{
    void *ptr;
    size_t capacity;
    size_t length;
}
buffer_t;

static void buffer_init(buffer_t *buffer, size_t initial_capacity)
{
    buffer->ptr = malloc(initial_capacity);
    buffer->capacity = initial_capacity;
    buffer->length = 0;
}

static void buffer_push(buffer_t *buffer, void *item, size_t size)
{
    size_t capacity = buffer->capacity;
    if (buffer->length + size > capacity)
    {
        // FIXME breakage if item is larger than capacity
        size_t new_capacity = capacity * 2;
        buffer->ptr = realloc(buffer->ptr, new_capacity);
        buffer->capacity = new_capacity;
    }
    
    size_t length = buffer->length;
    memcpy(buffer->ptr + length, item, size);
    buffer->length = length + size;
}

static void *buffer_pop(buffer_t *buffer, size_t size)
{
    size_t new_length = buffer->length - size;
    buffer->length = new_length;
    return buffer->ptr + new_length;
}

static inline void pack(uint8_t *buf, size_t n, ...)
{
    va_list args;
    va_start(args, n);
    
    size_t i;
    for (i = 0; i < n; ++i)
    {
        buf[i] = (uint8_t)va_arg(args, int);
    }
    
    va_end(args);
}

#define INSN(n, ...)            \
    pack(insn, n, __VA_ARGS__); \
    buffer_push(&fn, insn, n)

#define BYTESOF(x)    \
    (x >>  0) & 0xff, \
    (x >>  8) & 0xff, \
    (x >> 16) & 0xff, \
    (x >> 24) & 0xff

int main(int argc, char const *argv[])
{
    if (argc < 2) exit(1);
    char const *path = argv[1];
    FILE *in = fopen(path, "rb");
    if (!in) exit(1);
    
    char base[4096];
    strcpy(base, path);
    *strrchr(base, '.') = 0;
    
    uint8_t insn[8];
    buffer_t fn;
    buffer_init(&fn, 4096);
    
    size_t calloc_addr = (size_t)(void *)calloc;
    size_t putchar_addr = (size_t)(void *)putchar;
    size_t getchar_addr = (size_t)(void *)getchar;

    INSN(1, 0x55); // pushl %ebp
    INSN(2, 0x89, 0xe5); // movl %esp, %ebp
    INSN(1, 0x53); // pushl %ebx
    INSN(3, 0x83, 0xec, 0x08); // subl $8, %esp
    INSN(8, 0xc7, 0x44, 0x24, 0x04, 0x01, 0x00, 0x00, 0x00); // movl $1, 4(%esp)
    INSN(7, 0xc7, 0x04, 0x24, 0x30, 0x75, 0x00, 0x00); // movl $30000, (%esp)
    INSN(6, 0x8d, 0x15, BYTESOF(calloc_addr)); // leal _calloc, %edx
    INSN(2, 0xff, 0xd2); // call *%edx
    INSN(2, 0x89, 0xc3); // movl %eax, %ebx
    
    size_t loop, back, forward;
    buffer_t jump_stack;
    buffer_init(&jump_stack, 4096);
    
    int c;
    while ((c = fgetc(in)) != -1)
    {
        switch(c)
        {
        case '>':
            INSN(1, 0x43); // incl %ebx
            break;
        case '<':
            INSN(1, 0x4b); // decl %ebx
            break;
        case '+':
            INSN(2, 0xfe, 0x03); // incb (%ebx)
            break;
        case '-':
            INSN(2, 0xfe, 0x0b); // decb (%ebx)
            break;
        case '.':
            INSN(3, 0x0f, 0xb6, 0x03); // movzbl (%ebx), %eax
            INSN(3, 0x89, 0x04, 0x24); // movl %eax, (%esp)
            INSN(6, 0x8d, 0x15, BYTESOF(putchar_addr)); // leal _putchar, %edx
            INSN(2, 0xff, 0xd2); // call *%edx
            break;
        case ',':
            INSN(6, 0x8d, 0x15, BYTESOF(getchar_addr)); // leal _getchar, %edx
            INSN(2, 0xff, 0xd2); // call *%edx
            INSN(2, 0x88, 0x03); // movb %al, (%ebx)
            break;
        case '[':
            // save the loop top
            loop = fn.length;
            buffer_push(&jump_stack, &loop, sizeof(size_t));
            INSN(2, 0x8a, 0x03); // movb (%ebx), %al
            INSN(2, 0x84, 0xc0); // testb %al, %al
            // jump instruction will be filled later
            INSN(6, 0x0f, 0x84, 0x00, 0x00, 0x00, 0x00); // jel $0
            break;
        case ']':
            loop = *(size_t*)buffer_pop(&jump_stack, sizeof(size_t));
            back = loop - (fn.length + 5);
            INSN(5, 0xe9, BYTESOF(back)); // jmpl <loop top>
            // fill in the target of the forward jump
            forward = fn.length - (loop + 10);
            pack(fn.ptr + loop + 6, 4, BYTESOF(forward));
            break;
        }
    }
    
    // INSN(5, 0xb8, 0x00, 0x00, 0x00, 0x00); // movl $0, %eax
    INSN(6, 0x81, 0xc4, 0x08, 0x00, 0x00, 0x00); // addl $8, %esp
    INSN(1, 0x5b); // popl %ebx
    INSN(1, 0xc9); // leave
    INSN(1, 0xc3); // ret
    
    int result = mprotect(fn.ptr, fn.length, PROT_READ | PROT_EXEC);
    if (result != 0)
    {
        perror("mprotect");
        abort();
    }
    
    ((void (*)(void))(fn.ptr))();
    
    return EXIT_SUCCESS;
}
