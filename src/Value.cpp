/*-------------------------------------------------------------------------
    This source file is a part of m8rscript
    For the latest info, see http:www.marrin.org/
    Copyright (c) 2018-2019, Chris Marrin
    All rights reserved.
    Use of this source code is governed by the MIT license that can be
    found in the LICENSE file.
-------------------------------------------------------------------------*/

#include "Value.h"

#include "ExecutionUnit.h"
#include "Object.h"
#include "slre.h"

using namespace m8r;

m8r::String Value::toStringValue(ExecutionUnit* eu) const
{
    switch(type()) {
        case Type::None: return String("undefined");
        case Type::Function:
        case Type::Object: {
            Mad<Object> obj = asObject();
            return obj ? obj->toString(eu) : String("null");
        }
        case Type::Float: return String::toString(asFloatValue());
        case Type::Integer: return String::toString(asIntValue());
        case Type::String: {
            Mad<String> s = asString();
            return s ? *s : String("*BAD*");
        }
        case Type::StringLiteral: return eu->program()->stringFromStringLiteral(stringLiteralFromValue());
        case Type::Id: return String(eu->program()->stringFromAtom(atomFromValue()));
        case Type::Null: return String("null");
        case Type::NativeObject: return String("Native()"); // FIXME: Add formatted toString and show the address
        case Type::NativeFunction: return String("Callable()"); // FIXME: Add formatted toString and show the address
    }
}

Float Value::_toFloatValue(ExecutionUnit* eu) const
{
    switch(type()) {
        case Type::Function:
        case Type::Object: {
            Mad<Object> obj = asObject();
            Float f;
            if (obj) {
                String::toFloat(f, obj->toString(eu).c_str());
            }
            return f;
        }
        case Type::Float: return asFloatValue();
        case Type::Integer: return Float(int32FromValue(), 0);
        case Type::String: {
            const Mad<String> s = asString();
            if (!s) {
                return Float();
            }
            Float f;
            String::toFloat(f, s->c_str());
            return f;
        }
        case Type::StringLiteral: {
            const String& s = eu->program()->stringFromStringLiteral(stringLiteralFromValue());
            Float f;
            String::toFloat(f, s.c_str());
            return f;
        }
        case Type::Id:
        case Type::NativeObject:
        case Type::NativeFunction:
        case Type::Null:
            return Float();
        case Type::None:
            return Float::nan();
    }
}

Atom Value::_toIdValue(ExecutionUnit* eu) const
{
    switch(type()) {
        case Type::Function:
        case Type::Object: {
            Mad<Object> obj = asObject();
            return obj ? eu->program()->atomizeString(obj->toString(eu).c_str()) : Atom();
        }
        case Type::Integer:
        case Type::Float: return eu->program()->atomizeString(toStringValue(eu).c_str());
        case Type::String: {
            const Mad<String> s = asString();
            return s ? eu->program()->atomizeString(s->c_str()) : Atom();
        }
        case Type::StringLiteral: {
            const String& s = eu->program()->stringFromStringLiteral(stringLiteralFromValue());
            return eu->program()->atomizeString(s.c_str());
        }
        case Type::Id:
        case Type::NativeObject:
        case Type::NativeFunction:
        case Type::None:
        case Type::Null:
            return Atom();
    }
}

String Value::format(ExecutionUnit* eu, Value formatValue, uint32_t nparams)
{
    // thisValue is the format string
    String format = formatValue.toStringValue(eu);
    if (format.empty()) {
        return String();
    }
    
    String resultString;
    
    static const char* formatRegexROM = ROMSTR("(%)([\\d]*)(.?)([\\d]*)([c|s|d|i|x|X|u|f|e|E|g|G|p])");
        
    size_t formatRegexSize = ROMstrlen(formatRegexROM) + 1;
    Mad<char> formatRegex = Mallocator::shared()->allocate<char>(formatRegexSize);
    ROMmemcpy(formatRegex.get(), formatRegexROM, formatRegexSize);
    
    int32_t nextParam = 1 - nparams;

    int size = static_cast<int>(format.size());
    const char* start = format.c_str();
    const char* s = start;
    while (true) {
        struct slre_cap caps[5];
        memset(caps, 0, sizeof(caps));
        int next = slre_match(formatRegex.get(), s, size - static_cast<int>(s - start), caps, 5, 0);
        if (nextParam > 0 || next == SLRE_NO_MATCH) {
            // Print the remainder of the string
            resultString += s;
            formatRegex.destroy(formatRegexSize);
            return resultString;
        }
        if (next < 0) {
            formatRegex.destroy(formatRegexSize);
            return String();
        }
        
        // Output anything from s to the '%'
        assert(caps[0].len == 1);
        if (s != caps[0].ptr) {
            resultString += String(s, static_cast<int32_t>(caps[0].ptr - s));
        }
        
        // FIXME: handle the leading number(s) in the format
        assert(caps[4].len == 1);
        
        uint32_t width = 0;
        bool zeroFill = false;
        if (caps[1].len) {
            String::toUInt(width, caps[1].ptr);
            if (caps[1].ptr[0] == '0') {
                zeroFill = true;
            }
        }
        
        Value value = eu->stack().top(nextParam++);
        char formatChar = *(caps[4].ptr);
        
        switch (formatChar) {
            case 'c': {
                uint8_t uc = static_cast<char>(value.toIntValue(eu));
                char escapeChar = '\0';
                switch(uc) {
                    case 0x07: escapeChar = 'a'; break;
                    case 0x08: escapeChar = 'b'; break;
                    case 0x09: escapeChar = 't'; break;
                    case 0x0a: escapeChar = 'n'; break;
                    case 0x0b: escapeChar = 'v'; break;
                    case 0x0c: escapeChar = 'f'; break;
                    case 0x0d: escapeChar = 'r'; break;
                    case 0x1b: escapeChar = 'e'; break;
                }
                if (escapeChar) {
                    resultString += '\\';
                    resultString += escapeChar;
                } else if (uc < ' ') {
                    char buf[4] = "";
                    ::snprintf(buf, 3, "%02x", uc);
                    resultString += "\\x";
                    resultString += buf;
                } else {
                    resultString += static_cast<char>(uc);
                }
                break;
            }
            case 's':
                resultString += value.toStringValue(eu);
                break;
            case 'd':
            case 'i':
            case 'x':
            case 'X':
            case 'u': {
                String format = String("%") + (zeroFill ? "0" : "") + (width ? String::toString(width).c_str() : "");
                char numberBuf[20] = "";
                
                switch(formatChar) {
                    case 'd':
                    case 'i':
                        format += "d";
                        ::snprintf(numberBuf, 20, format.c_str(), value.toIntValue(eu));
                        break;
                    case 'x':
                    case 'X':
                        format += (formatChar == 'x') ? "x" : "X";
                        ::snprintf(numberBuf, 20, format.c_str(), static_cast<uint32_t>(value.toIntValue(eu)));
                        break;
                    case 'u':
                        format += "u";
                        ::snprintf(numberBuf, 20, format.c_str(), static_cast<uint32_t>(value.toIntValue(eu)));
                        break;
                }
                
                resultString += numberBuf;
                break;
            }
            case 'f':
            case 'e':
            case 'E':
            case 'g':
            case 'G':
                resultString += value.toStringValue(eu);
                break;
            case 'p': {
                char pointerBuf[20] = "";
                snprintf(pointerBuf, 20, "%p", *(reinterpret_cast<void**>(&value)));
                resultString += pointerBuf;
                break;
            }
            default:
                formatRegex.destroy(formatRegexSize);
                return String();
        }
        
        s += next;
    }
}

bool Value::isType(ExecutionUnit* eu, Atom atom)
{
    if (!isObject()) {
        return false;
    }
    Atom typeAtom = asObject()->typeName(eu);
    return typeAtom == atom;
}

bool Value::isType(ExecutionUnit* eu, SA sa)
{
    return isType(eu, Atom(sa));
}

const Value Value::property(ExecutionUnit* eu, const Atom& prop) const
{
    switch(type()) {
        case Type::Function:
        case Type::Object: {
            Mad<Object> obj = asObject();
            return obj ? obj->property(eu, prop) : Value();
        }
        case Type::Integer:
        case Type::Float: 
            // FIXME: Implement a Number object
            break;
        case Type::StringLiteral:
        case Type::String: {
            String s = toStringValue(eu);
            if (prop == Atom(SA::length)) {
                return Value(static_cast<int32_t>(s.size()));
            }
            break;
        }
        case Type::Id:
        case Type::NativeObject:
        case Type::NativeFunction:
        case Type::None:
        case Type::Null:
            break;
    }
    return Value();
}

bool Value::setProperty(ExecutionUnit* eu, const Atom& prop, const Value& value, Value::SetPropertyType type)
{
    // FIXME: Handle Integer, Float, String and StringLiteral
    Mad<Object> obj = asObject();
    return obj ? obj->setProperty(eu, prop, value, type) : false;
}

const Value Value::element(ExecutionUnit* eu, const Value& elt) const
{
    if (isString()) {
        // This means String or StringLiteral
        int32_t index = elt.toIntValue(eu);
        const Mad<String> s = asString();
        if (s) {
            if (s->size() > index && index >= 0) {
                return Value(static_cast<int32_t>((*s)[index]));
            }
        } else {
            // Must be a string literal
            const char* s = eu->program()->stringFromStringLiteral(asStringLiteralValue());
            if (s) {
                size_t size = strlen(s);
                if (size > index && index >= 0) {
                    return Value(static_cast<int32_t>((s[index])));
                }
            }
        }
    } else {
        Mad<Object> obj = asObject();
        if (obj) {
            return obj->element(eu, elt);
        }
    }
    return Value();
}

bool Value::setElement(ExecutionUnit* eu, const Value& elt, const Value& value, bool append)
{
    // FIXME: Handle Integer, Float, String and StringLiteral
    Mad<Object> obj = asObject();
    return obj ? obj->setElement(eu, elt, value, append) : false;
}

CallReturnValue Value::call(ExecutionUnit* eu, Value thisValue, uint32_t nparams, bool ctor)
{
    // FIXME: Handle Integer, Float, String and StringLiteral
    if (isNativeFunction()) {
        return asNativeFunction()(eu, thisValue, nparams);
    }
    Mad<Object> obj = asObject();
    return obj ? obj->call(eu, thisValue, nparams, ctor) : CallReturnValue(CallReturnValue::Error::CannotCall);
}

CallReturnValue Value::callProperty(ExecutionUnit* eu, Atom prop, uint32_t nparams)
{
    switch(type()) {
        case Type::Function:
        case Type::Object: {
            Mad<Object> obj = asObject();
            return obj ? obj->callProperty(eu, prop, nparams) : CallReturnValue(CallReturnValue::Error::CannotCall);
        }
        case Type::Integer:
        case Type::Float: 
            // FIXME: Implement a Number object
            return CallReturnValue(CallReturnValue::Error::CannotCall);
        case Type::StringLiteral:
        case Type::String: {
            String s = toStringValue(eu);
            if (prop == Atom(SA::format)) {
                String s = Value::format(eu, eu->stack().top(1 - nparams), nparams - 1);
                eu->stack().push(Value(s));
                return CallReturnValue(CallReturnValue::Type::ReturnCount, 1);
            }
            if (prop == Atom(SA::trim)) {
                s = s.trim();
                eu->stack().push(Value(Object::createString(s)));
                return CallReturnValue(CallReturnValue::Type::ReturnCount, 1);
            }
            if (prop == Atom(SA::split)) {
                String separator = (nparams > 0) ? eu->stack().top(1 - nparams).toStringValue(eu) : String(" ");
                bool skipEmpty = (nparams > 1) ? eu->stack().top(2 - nparams).toBoolValue(eu) : false;
                Vector<String> array = s.split(separator, skipEmpty);
                Mad<MaterObject> arrayObject = Mad<MaterObject>::create();
                arrayObject->setArray(true);
                arrayObject->resize(array.size());
                for (size_t i = 0; i < array.size(); ++i) {
                    (*arrayObject)[i] = Value(Object::createString(array[i]));
                }
                
                eu->stack().push(Value(Mad<Object>(static_cast<Mad<Object>>(arrayObject))));
                return CallReturnValue(CallReturnValue::Type::ReturnCount, 1);
            }
            return CallReturnValue(CallReturnValue::Error::PropertyDoesNotExist);
        }
        case Type::Id:
        case Type::NativeObject:
        case Type::NativeFunction:
        case Type::None:
        case Type::Null:
            return CallReturnValue(CallReturnValue::Error::CannotCall);
    }
}

void Value::gcMark()
{
    Mad<String> string = asString();
    if (string) {
        string->setMarked(true);
        return;
    }
    
    Mad<Object> obj = asObject();
    if (obj) {
        obj->gcMark();
    }
}
