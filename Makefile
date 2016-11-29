#
# clang++ -std=c++11 -stdlib=libc++ -Weverything -Isrc src/main.cpp
# 
# 
SRC_PATH=./src
OUT_PATH=./bin
VPATH=${SRC_PATH}
#FILES= ./src/crypto/aes.cpp ./src/main.cpp 
FILES= ./src/main.cpp
SOURCES = $(FILES:%.cpp=$(SRC_PATH)/%.cpp)

#CC=g++
CC=clang++
#CFLAGS=-Wall -shared
CFLAGS=-Wall 

all:
	#mkdir -p ${OUT_PATH}
	#${CC} ${CFLAGS} -I${SRC_PATH} ${FILES} -o ${OUT_PATH}/Maple
	${CC} -std=c++11 -stdlib=libc++ -Wdeprecated -Wc++98-compat -Weverything  -I${SRC_PATH} ${FILES} -o ${OUT_PATH}/Maple	






