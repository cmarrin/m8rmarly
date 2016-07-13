/*-------------------------------------------------------------------------
This source file is a part of m8rscript

For the latest info, see http://www.marrin.org/

Copyright (c) 2016, Chris Marrin
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

    - Redistributions of source code must retain the above copyright notice, 
	  this list of conditions and the following disclaimer.
	  
    - Redistributions in binary form must reproduce the above copyright 
	  notice, this list of conditions and the following disclaimer in the 
	  documentation and/or other materials provided with the distribution.
	  
    - Neither the name of the <ORGANIZATION> nor the names of its 
	  contributors may be used to endorse or promote products derived from 
	  this software without specific prior written permission.
	  
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
POSSIBILITY OF SUCH DAMAGE.
-------------------------------------------------------------------------*/

#include "Function.h"

#include "ExecutionUnit.h"

using namespace m8r;

int32_t Function::call(Program* program, ExecutionUnit* eu, uint32_t nparams)
{
    return eu->run(program, this, nparams);
}

int32_t Function::addLocal(const Atom& atom)
{
    for (auto name : _locals) {
        if (name == atom) {
            return -1;
        }
    }
    _locals.push_back(atom);
    return static_cast<int32_t>(_locals.size()) - 1;
}

int32_t Function::localIndex(const Atom& name) const
{
    for (int32_t i = 0; i < static_cast<int32_t>(_locals.size()); ++i) {
        if (_locals[i] == name) {
            return i;
        }
    }
    return -1;
}

bool Function::serialize(Stream* stream, Error& error) const
{
    // FIXME: Finish implementation
    return serializeCode(stream, error);
}

bool Function::deserialize(Stream* stream, Error& error)
{
    // FIXME: Finish implementation
    return deserializeCode(stream, error);
}

bool Function::serializeCode(Stream* stream, Error& error) const
{
    size_t size = _code.size();
    return serializeBuffer(stream, error, ObjectDataType::Code, &(_code[0]), size);
}

bool Function::deserializeCode(Stream* stream, Error& error)
{
    _code.clear();

    uint16_t size;
    if (!deserializeBufferSize(stream, error, ObjectDataType::Code, size)) {
        return false;
    }
    
    _code.resize(size);
    return deserializeBuffer(stream, error, &(_code[0]), size);
}
