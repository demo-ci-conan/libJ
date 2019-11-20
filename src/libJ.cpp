
#include "libJ/libJ.h"

#include <iostream>

void hello_libJ(int indent, const std::string& msg) {
    std::cout << std::string(indent, ' ') << "libJ: " << msg << std::endl;
}