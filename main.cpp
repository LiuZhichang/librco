//
// Created by root on 2022/3/21.
//

#include <iostream>

#include "rco.h"

int main(int argc, char** argv) {

    //rco::Runtime::Set_GC_threshold(10);

    for(int i = 1; i <= 3000; ++i) {
        rco_exec [=]{
            std::cout << "co " << i << " exec" << std::endl;
        };
    }

    rco_sched.start(2, 0);
    std::cout << "main finish" << std::endl;

    return 0;
}
