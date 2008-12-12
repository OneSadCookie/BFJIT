#!/usr/bin/ruby
# usage: ./bf2s.rb <file>.bf

path = ARGV[0]
path_base = path[0...-File.extname(path).length]
bf = File.read(path)

File.open(path_base + '.s', 'wb') do |f|
    f.puts <<EOH
    .text
.globl _main
_main:
    pushl   %ebp
    movl    %esp, %ebp
    subl    $40, %esp
    movl    $1, 4(%esp)
    movl    $30000, (%esp)
    call    _calloc
    movl    %eax, %ebx
    
    
    
EOH
    
    label = 1
    label_stack = []
    
    bf.each_byte do |b|
        case b
        when ?>
            f.puts("    inc     %ebx")
        when ?<
            f.puts("    dec     %ebx")
        when ?+
            f.puts("    incb    (%ebx)")
        when ?-
            f.puts("    decb    (%ebx)")
        when ?.
            f.puts("    movzbl  (%ebx), %eax")
            f.puts("    movl    %eax, (%esp)")
            f.puts("    call    _putchar")
        when ?,
            f.puts("    call    _getchar")
            f.puts("    movb    %al, (%ebx)")
        when ?[
            f.puts("L#{label}:")
            f.puts("    movb    (%ebx), %al")
            f.puts("    testb   %al, %al")
            f.puts("    je      L#{label}_exit")
            label_stack << label
            label += 1
        when ?]
            f.puts("    jmp     L#{label_stack[-1]}")
            f.puts("L#{label_stack[-1]}_exit:")
            label_stack.pop()
        end
    end
    
    f.puts <<EOF
    
    
    
    movl    $0, %eax
    leave
    ret
    .subsections_via_symbols
EOF
end

system("gcc -arch i386 '#{path_base}.s' -o #{path_base}")
