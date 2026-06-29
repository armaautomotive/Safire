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

FILES_MAC= ./src/main.cpp ./src/platform.cpp ./src/ecdsacrypto.cpp ./src/wallet.cpp ./src/transaction.cpp ./src/userdb.cpp ./src/blockdb.cpp ./src/networktime.cpp ./src/cli.cpp ./src/network/relayclient.cpp ./src/network/localpeerclient.cpp ./src/network/server.cpp ./src/network/connection.cpp ./src/network/request_handler.cpp ./src/network/request_parser.cpp ./src/network/reply.cpp ./src/network/mime_types.cpp ./src/log.cpp ./src/network/p2p.cpp ./src/functions/*.cpp ./src/compression.cpp ./src/base64.cpp ./src/heartbeat.cpp ./src/carryforward.cpp 
# ./src/network/*.cpp 
# ./src/network/p2p.cpp  
# ./src/util.cpp ./src/rsacrypto.cpp  
#	./src/wallet/wallet.cpp
FILES_LINUX= ./src/main.cpp ./src/platform.cpp ./src/ecdsacrypto.cpp ./src/wallet.cpp ./src/transaction.cpp ./src/userdb.cpp ./src/blockdb.cpp ./src/networktime.cpp ./src/cli.cpp ./src/log.cpp  ./src/functions/*.cpp  ./src/network/relayclient.cpp ./src/network/localpeerclient.cpp ./src/network/server.cpp ./src/network/connection.cpp ./src/network/request_handler.cpp ./src/network/request_parser.cpp ./src/network/reply.cpp ./src/network/mime_types.cpp ./src/network/p2p.cpp ./src/compression.cpp ./src/base64.cpp ./src/heartbeat.cpp ./src/carryforward.cpp
#  ./src/network/*.cpp   This fails to link because of boost...
FILES_LINUX2= ./src/main2.cpp  ./src/ecdsacrypto.cpp   
SOURCES = $(FILES:%.cpp=$(SRC_PATH)/%.cpp)

.PHONY: all mac linux linux2 clean

#  gcc -o yourname -Bstatic -L<dir-of-libcrypto.a> -lcrypto . . . yourfile.c
# MacOS Doesn’t support static linking.
CFLAGS_MAC= -arch x86_64 -std=c++11 -stdlib=libc++ -Wdeprecated -Wc++98-compat -w    `pkg-config --cflags nice`
LIBS_MAC= -L/usr/local/opt/openssl/lib -L/usr/local/lib /usr/local/opt/leveldb/lib/libleveldb.a -lssl -lcrypto -lboost_system -lboost_thread-mt -lboost_filesystem -lsnappy -lcurl -lz   `pkg-config --libs nice`
# -lsecp256k1
# -lboost_system -lboost_asio
# -L./usr/local/Cellar/boost/1.62.0/lib

#  /usr/local/lib/libsecp256k1.a
# -Bstatic
# -lboost_system-mt not found
CFLAGS_LINUX= -pthread -Wdeprecated -w  `pkg-config --cflags nice`
LIBS_LINUX= -L/usr/local/lib/  -L./src/leveldb  -L/usr/lib/x86_64-linux-gnu/ -pthread  -lboost_system  -lssl -lcrypto -lboost_filesystem -lleveldb -lcurl -lz  `pkg-config --libs nice`
# -L/usr/lib/
# -lsecp256k1
# -Weverything  
# -std=gnu99  


all:
	mkdir -p ${OUT_PATH}
	${CC_MAC} ${CFLAGS_MAC} ${SRC_PATH_MAC} ${FILES_MAC} ${LIBS_MAC} -o ${OUT_PATH}/Safire	

mac:
	mkdir -p ${OUT_PATH}	
	${CC_MAC} ${CFLAGS_MAC} ${SRC_PATH_MAC} ${FILES_MAC} ${LIBS_MAC} -o ${OUT_PATH}/Safire 

linux:
	mkdir -p ${OUT_PATH}
	${CC_MAC} ${CFLAGS_LINUX} -std=c++11 ${SRC_PATH_LINUX} ${FILES_LINUX} ${LIBS_LINUX} -o ${OUT_PATH}/Safire	

linux2: 
	clang++ ${CFLAGS_LINUX} -std=c++11 ${SRC_PATH_LINUX} ${FILES_LINUX2} -lssl -o ${OUT_PATH}/Safire	

clean:
	rm -f ${OUT_PATH}/Safire
	rm -f ./src/*.o ./gui_client/*.o ./gui_client/moc_*.cpp ./gui_client/moc_*.o ./gui_client/moc_predefs.h

# TODO add tests application target to run all unit tests.
