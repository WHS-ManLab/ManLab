#pragma once
#include <ostream>

class FimCommandHandler 
{
public:
    void IntScan(std::ostream& out);
    void BaselineGen(std::ostream& out);
    void PrintBaseline(std::ostream& out);
    void PrintIntegscan(std::ostream& out);
};