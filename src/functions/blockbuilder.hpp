//
//  blockbuilder.hpp
//  
//
//  Created by Jon on 2018-02-13.
//

#ifndef blockbuilder_hpp
#define blockbuilder_hpp

#include <stdio.h>
#include <thread>

class CBlockBuilder
{
private:
    
public:
    CBlockBuilder()
    {
        
    }
    
    ~CBlockBuilder()
    {
    }
    
    void blockBuilderThread(int argc, char* argv[]);
    void stop();
};

#endif /* blockbuilder_hpp */
