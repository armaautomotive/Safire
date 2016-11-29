
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <iostream>
#include "crypto/common.h"
#include "crypto/sha256.h"
#include "wallet/wallet.h"

static const uint64_t BUFFER_SIZE = 1000*1000; // Temp

int main()
{
	std::cout << "Maple Digital Currency v0.0.1" << std::endl;

	//sha256::
	//CSHA256 sha;
	uint8_t hash[CSHA256::OUTPUT_SIZE];
	std::vector<uint8_t> in(BUFFER_SIZE, 0);
	//while (state.KeepRunning())
        //CSHA256().Write(begin_ptr(in), in.size()).Finalize(hash);

	std::string message("Test");



}

