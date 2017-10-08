#
# clang++ -std=c++11 -stdlib=libc++ -Weverything -Isrc src/main.cpp
# 
# 
SRC_PATH=./src
SRC_PATH_LINUX=-I./src -I/usr/local/include -I./src/leveldb/include
OUT_PATH=./bin
VPATH=${SRC_PATH}
#FILES= ./src/crypto/aes.cpp ./src/main.cpp 
#FILES= ./src/main.cpp ./src/crypto/sha256.cpp ./src/crypto/hmac_sha256.cpp ./src/support/cleanse.cpp ./src/support/lockedpool.cpp  ./src/key.cpp ./src/pubkey.cpp ./src/random.cpp ./src/crypto/sha512.cpp ./src/hash.cpp ./src/crypto/ripemd160.cpp ./src/crypto/hmac_sha512.cpp  ./src/ecdsacrypto.cpp 

FILES= ./src/main.cpp ./src/ecdsacrypto.cpp ./src/wallet.cpp ./src/transaction.cpp ./src/userdb.cpp ./src/blockdb.cpp ./src/networktime.cpp ./src/cli.cpp ./src/network/*.cpp ./src/functions/*.cpp  
# ./src/util.cpp ./src/rsacrypto.cpp  
#	./src/wallet/wallet.cpp
FILES_LINUX= ./src/main.cpp ./src/ecdsacrypto.cpp ./src/wallet.cpp ./src/transaction.cpp ./src/networktime.cpp ./src/cli.cpp ./src/network/*.cpp ./src/functions/*.cpp
SOURCES = $(FILES:%.cpp=$(SRC_PATH)/%.cpp)

CC_LINUX=g++
CC=clang++

#  gcc -o yourname -Bstatic -L<dir-of-libcrypto.a> -lcrypto . . . yourfile.c
# MacOS Doesn’t support static linking.
CFLAGS_MAC= -I/usr/local/opt/openssl/include -I/usr/local/include -I/usr/local/Cellar/boost/1.62.0/include -I./src/leveldb/include -Bstatic -L/usr/local/opt/openssl/lib -Bstatic -L/usr/local/lib -L./src/leveldb  -lssl -lcrypto -lboost_system -lboost_thread-mt -lleveldb -std=c++11 -stdlib=libc++ -Wdeprecated -Wc++98-compat -w 
# -lsecp256k1
# -lboost_system -lboost_asio
# -L./usr/local/Cellar/boost/1.62.0/lib

#  /usr/local/lib/libsecp256k1.a
# -Bstatic
CFLAGS_LINUX= -L/usr/local/lib/ -L/usr/lib/ -lssl -lcrypto -lboost_filesystem -lboost_system -lsecp256k1 -Wdeprecated -w 
# -Weverything  
# -std=gnu99  


all:
	#mkdir -p ${OUT_PATH}
	${CC} ${CFLAGS_MAC} -I${SRC_PATH} ${FILES} -o ${OUT_PATH}/Safire	

mac:
	${CC} ${CFLAGS_MAC} -I${SRC_PATH} ${FILES} -o ${OUT_PATH}/Safire 

linux:
	${CC_LINUX} ${CFLAGS_LINUX} -std=c++11 ${SRC_PATH_LINUX} ${FILES_LINUX} -o ${OUT_PATH}/Safire	


# TODO add tests application target to run all unit tests.
