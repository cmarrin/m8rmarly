#include "Parser.h"

void print(const char* s) { }

void setup()
{
    m8r::FileStream istream("");
    m8r::Parser parser(&istream, ::print);
    m8r::ExecutionUnit eu;
    eu.generateCodeString(parser.program()).c_str();
    eu.run(parser.program(), print);

    //m8r::Program program;
    //eu.run(&program, print);
}

void loop()
{
}
