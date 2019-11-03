/*-------------------------------------------------------------------------
    This source file is a part of m8rscript
    For the latest info, see http:www.marrin.org/
    Copyright (c) 2018-2019, Chris Marrin
    All rights reserved.
    Use of this source code is governed by the MIT license that can be
    found in the LICENSE file.
-------------------------------------------------------------------------*/

#include "Iterator.h"

#include "ExecutionUnit.h"
#include "Program.h"
#include "SystemInterface.h"

using namespace m8r;

Iterator::Iterator(Program* program, ObjectFactory* parent)
    : ObjectFactory(program, SA::Iterator, parent, constructor)
{
    addProperty(program, SA::done, done);
    addProperty(program, SA::next, next);
    addProperty(program, SA::getValue, getValue);
    addProperty(program, SA::setValue, setValue);
}

static bool done(ExecutionUnit* eu, Value thisValue, Object*& obj, int32_t& index)
{
    obj = thisValue.property(eu, Atom(SA::__object)).asObject();
    index = thisValue.property(eu, Atom(SA::__index)).asIntValue();
    if (!obj) {
        return true;
    }
    int32_t size = obj->property(eu, Atom(SA::length)).asIntValue();
    return index >= size;
}

CallReturnValue Iterator::constructor(ExecutionUnit* eu, Value thisValue, uint32_t nparams)
{
    if (nparams < 1) {
        return CallReturnValue(CallReturnValue::Error::WrongNumberOfParams);
    }
    
    Object* obj = eu->stack().top(1 - nparams).asObject();
    if (!obj) {
        return CallReturnValue(CallReturnValue::Error::InvalidArgumentValue);
    }
    
    thisValue.setProperty(eu, Atom(SA::__object), Value(obj), Value::SetPropertyType::AlwaysAdd);
    thisValue.setProperty(eu, Atom(SA::__index), Value(0), Value::SetPropertyType::AlwaysAdd);
    
    return CallReturnValue(CallReturnValue::Type::ReturnCount, 0);
}

CallReturnValue Iterator::done(ExecutionUnit* eu, Value thisValue, uint32_t nparams)
{
    Object* obj;
    int32_t index;
    eu->stack().push(Value(::done(eu, thisValue, obj, index)));
    return CallReturnValue(CallReturnValue::Type::ReturnCount, 1);
}

CallReturnValue Iterator::next(ExecutionUnit* eu, Value thisValue, uint32_t nparams)
{
    Object* obj;
    int32_t index;
    if (!::done(eu, thisValue, obj, index)) {
        ++index;
        if (!thisValue.setProperty(eu, Atom(SA::__index), Value(index), Value::SetPropertyType::NeverAdd)) {
            return CallReturnValue(CallReturnValue::Error::InternalError);
        }
    }
    return CallReturnValue(CallReturnValue::Type::ReturnCount, 0);
}

CallReturnValue Iterator::getValue(ExecutionUnit* eu, Value thisValue, uint32_t nparams)
{
    Object* obj;
    int32_t index;
    if (!::done(eu, thisValue, obj, index)) {
        eu->stack().push(obj->element(eu, Value(index)));
    }
    return CallReturnValue(CallReturnValue::Type::ReturnCount, 1);
}

CallReturnValue Iterator::setValue(ExecutionUnit* eu, Value thisValue, uint32_t nparams)
{
    if (nparams < 1) {
        return CallReturnValue(CallReturnValue::Error::WrongNumberOfParams);
    }
    
    Object* obj;
    int32_t index;
    if (!::done(eu, thisValue, obj, index)) {
        obj->setElement(eu, Value(index), eu->stack().top(1 - nparams), false);
    }
    return CallReturnValue(CallReturnValue::Type::ReturnCount, 0);
}
