// Copyright (c) 2016 Jon Taylor
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "networktime.h"
#include <sstream>
#include <string>
#include <unistd.h>   // open and close
#include <sys/stat.h> // temp because we removed util
#include <fcntl.h> // temp removed util.h
#include <time.h>

#include "boost/date_time/posix_time/posix_time.hpp"

long CNetworkTime::getEpoch(){
	boost::posix_time::ptime const time_epoch(boost::gregorian::date(1970, 1, 1));
	long ms = (boost::posix_time::microsec_clock::local_time() - time_epoch).total_microseconds();
	return ms;

	//timeval curTime;
	//gettimeofday(&curTime, NULL);
	//int milli = curTime.tv_usec / 1000;
	//long blockNumber = milli / (60000 * 10);
	//return milli;
}

