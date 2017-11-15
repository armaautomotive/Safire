
#gcc -o -I./agent/ simple-example simple-example.c `pkg-config --cflags --libs nice`

gcc -o simple-example simple-example.c `pkg-config --cflags --libs nice`
