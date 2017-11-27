#
# clang++ -std=c++11 -stdlib=libc++ -Weverything -Isrc src/main.cpp
# 
#
CC_LINUX=g++
CC_MAC=clang++

OUT_PATH=./
 
SRC_PATH_MAC=-I./ 
SRC_PATH_LINUX=-I./ 

FILES_MAC= ./simple-example-2.cpp  
FILES_LINUX= ./simple-example-2.cpp 
SOURCES = $(FILES:%.cpp=$(SRC_PATH)/%.cpp)

#  gcc -o yourname -Bstatic -L<dir-of-libcrypto.a> -lcrypto . . . yourfile.c
# MacOS Doesn’t support static linking.
CFLAGS_MAC=  -std=c++11 -stdlib=libc++ -Wdeprecated -Wc++98-compat -w    `pkg-config --cflags --libs nice`  

#  /usr/local/lib/libsecp256k1.a
CFLAGS_LINUX= -L/usr/local/lib/  -L/usr/lib/x86_64-linux-gnu/ -pthread   -Wdeprecated -w  `pkg-config --cflags --libs nice` 
# -std=gnu99  


all:
	mkdir -p ${OUT_PATH}
	${CC_MAC} ${CFLAGS_MAC} ${SRC_PATH_MAC} ${FILES_MAC} -o ${OUT_PATH}/simple-example-2	

mac:
	mkdir -p ${OUT_PATH}	
	${CC_MAC} ${CFLAGS_MAC} ${SRC_PATH_MAC} ${FILES_MAC} -o ${OUT_PATH}/simple-example-2

linux:
	mkdir -p ${OUT_PATH}
	${CC_MAC} ${CFLAGS_LINUX} -std=c++11 ${SRC_PATH_LINUX} ${FILES_LINUX} -o ${OUT_PATH}/simple-example-2	

linux2: 
	clang++ ${CFLAGS_LINUX} -std=c++11 ${SRC_PATH_LINUX} ${FILES_LINUX2} -lssl -o ${OUT_PATH}/simple-example-2	

