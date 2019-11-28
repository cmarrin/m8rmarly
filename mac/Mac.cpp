/*-------------------------------------------------------------------------
    This source file is a part of m8rscript
    For the latest info, see http:www.marrin.org/
    Copyright (c) 2018-2019, Chris Marrin
    All rights reserved.
    Use of this source code is governed by the MIT license that can be
    found in the LICENSE file.
-------------------------------------------------------------------------*/

#include "Mac.h"

#include "GPIOInterface.h"
#include "MacTaskManager.h"
#include "MacTCP.h"
#include "MacUDP.h"
#include "SystemInterface.h"

#ifndef USE_LITTLEFS
#include "SpiffsFS.h"
#else
#include "LittleFS.h"
#endif

using namespace m8r;

class MacSystemInterface : public m8r::SystemInterface
{
public:
    void init(const char* fsFile) { _fileSystem.init(fsFile); }
    
    virtual void vprintf(m8r::ROMString s, va_list args) const override
    {
        m8r::String ss(s);
        ::vprintf(ss.c_str(), args);
    }
    
    virtual void setDeviceName(const char*) override { }
    virtual m8r::FS* fileSystem() override { return &_fileSystem; }
    virtual m8r::GPIOInterface* gpio() override { return &_gpio; }
    virtual m8r::TaskManager* taskManager() override { return &_taskManager; };
    
    virtual m8r::Mad<m8r::TCP> createTCP(m8r::TCPDelegate* delegate, uint16_t port, m8r::IPAddr ip = m8r::IPAddr()) override
    {
        m8r::Mad<m8r::MacTCP> tcp = m8r::Mad<m8r::MacTCP>::create(m8r::MemoryType::Network);
        tcp->init(delegate, port, ip);
        return tcp;
    }
    
    virtual m8r::Mad<m8r::UDP> createUDP(m8r::UDPDelegate* delegate, uint16_t port) override
    {
        m8r::Mad<m8r::MacUDP> udp = m8r::Mad<m8r::MacUDP>::create(m8r::MemoryType::Network);
        udp->init(delegate, port);
        return udp;
    }

private:
    class MyGPIOInterface : public m8r::GPIOInterface {
    public:
        MyGPIOInterface() { }
        virtual ~MyGPIOInterface() { }

        virtual bool setPinMode(uint8_t pin, PinMode mode) override
        {
            ::printf("*** setPinMode(%d, %d)\n", pin, static_cast<uint32_t>(mode));
            _pinio = (_pinio & ~(1 << pin)) | ((mode == PinMode::Output) ? (1 << pin) : 0);
            return true;
        }
        
        virtual bool digitalRead(uint8_t pin) const override
        {
            ::printf("*** digitalRead(%d) ==> %s\n", pin, (_pinstate & (1 << pin)) ? "true" : "false");
            return _pinstate & (1 << pin);
        }
        
        virtual void digitalWrite(uint8_t pin, bool level) override
        {
            if (pin > 16) {
                return;
            }
            _pinstate = (_pinstate & ~(1 << pin)) | (level ? (1 << pin) : 0);
            ::printf("*** digitalWrite(%d, %s)\n", pin, level ? "true" : "false");
        }
        
        virtual void onInterrupt(uint8_t pin, Trigger, std::function<void(uint8_t pin)> = { }) override { }
        
    private:
        // 0 = input, 1 = output
        uint32_t _pinio = 0;
        uint32_t _pinstate = 0;
    };
    
    MyGPIOInterface _gpio;
#ifndef USE_LITTLEFS
    m8r::SpiffsFS _fileSystem;
#else
    m8r::LittleFS _fileSystem;
#endif
    m8r::MacTaskManager _taskManager;
};

uint64_t m8r::SystemInterface::currentMicroseconds()
{
    return static_cast<uint64_t>(std::clock() * 1000000 / CLOCKS_PER_SEC);
}

void m8r::SystemInterface::heapInfo(void*& start, uint32_t& size)
{
    static void* heap = nullptr;
    if (!heap) {
        heap = ::malloc(HeapSize);
    }
    start = heap;
    size = HeapSize;
}

static std::mutex _gcLockMutex;
static std::mutex _mallocatorLockMutex;
static std::mutex _eventLockMutex;

void SystemInterface::gcLock()
{
    _gcLockMutex.lock();
}

void SystemInterface::gcUnlock()
{
    _gcLockMutex.unlock();
}

void SystemInterface::mallocatorLock()
{
    _mallocatorLockMutex.lock();
}

void SystemInterface::mallocatorUnlock()
{
    _mallocatorLockMutex.unlock();
}

void SystemInterface::eventLock()
{
    _eventLockMutex.lock();
}

void SystemInterface::eventUnlock()
{
    _eventLockMutex.unlock();
}

static MacSystemInterface _gSystemInterface;

m8r::SystemInterface* m8r::SystemInterface::get() { return &_gSystemInterface; }
void m8r::initMacSystemInterface(const char* fsFile) { _gSystemInterface.init(fsFile); }