#
# clang++ -std=c++11 -stdlib=libc++ -Weverything -Isrc src/main.cpp
# 
#
CC_LINUX=g++
CC_MAC=clang++

OUT_PATH=./bin
 
SRC_PATH_MAC=-I./src -I/usr/local/opt/openssl/include -I/usr/local/include -I/usr/local/Cellar/boost/1.62.0/include -I./src/leveldb/include
SRC_PATH_LINUX=-I./src -I/usr/include/openssl -I/usr/local/include -I./src/leveldb/include
#FILES= ./src/crypto/aes.cpp ./src/main.cpp 
#FILES= ./src/main.cpp ./src/crypto/sha256.cpp ./src/crypto/hmac_sha256.cpp ./src/support/cleanse.cpp ./src/support/lockedpool.cpp  ./src/key.cpp ./src/pubkey.cpp ./src/random.cpp ./src/crypto/sha512.cpp ./src/hash.cpp ./src/crypto/ripemd160.cpp ./src/crypto/hmac_sha512.cpp  ./src/ecdsacrypto.cpp 

FILES_MAC= ./src/main.cpp ./src/ecdsacrypto.cpp ./src/wallet.cpp ./src/transaction.cpp ./src/userdb.cpp ./src/blockdb.cpp ./src/networktime.cpp ./src/cli.cpp ./src/network/p2p.cpp ./src/functions/*.cpp 
# ./src/network/*.cpp 
# ./src/network/p2p.cpp  
# ./src/util.cpp ./src/rsacrypto.cpp  
#	./src/wallet/wallet.cpp
FILES_LINUX= ./src/main.cpp ./src/ecdsacrypto.cpp ./src/wallet.cpp ./src/transaction.cpp ./src/networktime.cpp ./src/cli.cpp  ./src/functions/*.cpp   ./src/network/p2p.cpp
#  ./src/network/*.cpp   This fails to link because of boost...
FILES_LINUX2= ./src/main2.cpp  ./src/ecdsacrypto.cpp   
SOURCES = $(FILES:%.cpp=$(SRC_PATH)/%.cpp)

#  gcc -o yourname -Bstatic -L<dir-of-libcrypto.a> -lcrypto . . . yourfile.c
# MacOS Doesn’t support static linking.
CFLAGS_MAC= -L/usr/local/opt/openssl/lib -Bstatic -L/usr/local/lib -L./src/leveldb  -lssl -lcrypto -lboost_system -lboost_thread-mt -lleveldb    -std=c++11 -stdlib=libc++ -Wdeprecated -Wc++98-compat -w    `pkg-config --cflags --libs nice`  
# -lsecp256k1
# -lboost_system -lboost_asio
# -L./usr/local/Cellar/boost/1.62.0/lib

#  /usr/local/lib/libsecp256k1.a
# -Bstatic
# -lboost_system-mt not found
CFLAGS_LINUX= -L/usr/local/lib/  -L/usr/lib/x86_64-linux-gnu/ -pthread  -lboost_system  -lssl -lcrypto -lboost_filesystem -Wdeprecated -w  `pkg-config --cflags --libs nice` 
# -L/usr/lib/
# -lsecp256k1
# -Weverything  
# -std=gnu99  


all:
	mkdir -p ${OUT_PATH}
	${CC_MAC} ${CFLAGS_MAC} ${SRC_PATH_MAC} ${FILES_MAC} -o ${OUT_PATH}/Safire	

mac:
	mkdir -p ${OUT_PATH}	
	${CC_MAC} ${CFLAGS_MAC} ${SRC_PATH_MAC} ${FILES_MAC} -o ${OUT_PATH}/Safire 

linux:
	mkdir -p ${OUT_PATH}
	${CC_MAC} ${CFLAGS_LINUX} -std=c++11 ${SRC_PATH_LINUX} ${FILES_LINUX} -o ${OUT_PATH}/Safire	

linux2: 
	clang++ ${CFLAGS_LINUX} -std=c++11 ${SRC_PATH_LINUX} ${FILES_LINUX2} -lssl -o ${OUT_PATH}/Safire	

# TODO add tests application target to run all unit tests.
