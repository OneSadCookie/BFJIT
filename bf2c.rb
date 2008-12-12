#!/usr/bin/ruby

def indent(n)
    '    ' * n
end

path = ARGV[0]
path_base = path[0...-File.extname(path).length]
bf = File.read(path)

File.open(path_base + '.c', 'wb') do |f|
    f.puts <<EOH
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    char *p = calloc(30000, 1);
EOH
    
    level = 1
    
    bf.each_byte do |b|
        case b
        when ?>
            f.puts(indent(level) + "++p;")
        when ?<
            f.puts(indent(level) + "--p;")
        when ?+
            f.puts(indent(level) + "++*p;")
        when ?-
            f.puts(indent(level) + "--*p;")
        when ?.
            f.puts(indent(level) + "putchar(*p);")
        when ?,
            f.puts(indent(level) + "*p = getchar();")
        when ?[
            f.puts(indent(level) + "while (*p)")
            f.puts(indent(level) + "{")
            level += 1
        when ?]
            level -= 1
            f.puts(indent(level) + "}")
        end
    end
    
    f.puts <<EOF
    return EXIT_SUCCESS;
}
EOF
end

system("gcc -Wall -Wextra -Werror '#{path_base}.c' -o #{path_base}")
