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

#include "SpiffsFS.h"

#include "spiffs_nucleus.h"
#include "Application.h"

using namespace m8r;

spiffs SpiffsFS::_spiffsFileSystem;


SpiffsFS::SpiffsFS(const char* name)
{
    memset(&_spiffsFileSystem, 0, sizeof(_spiffsFileSystem));
    _spiffsWorkBuf = new uint8_t[SPIFFS_CFG_LOG_PAGE_SZ() * 2];
    assert(_spiffsWorkBuf);
    
    setConfig(_config, name);
}

SpiffsFS::~SpiffsFS()
{
    SPIFFS_unmount(&_spiffsFileSystem);
    delete [ ] _spiffsWorkBuf;
}

bool SpiffsFS::mount()
{
    system()->printf(ROMSTR("Mounting SPIFFS...\n"));
    int32_t result = internalMount();
    if (result != SPIFFS_OK) {
        if (result == SPIFFS_ERR_NOT_A_FS) {
            system()->printf(ROMSTR("ERROR: Not a valid SPIFFS filesystem. Please format.\n"));
            _error = Error::Code::FSNotFormatted;
        } else {
            system()->printf(ROMSTR("ERROR: SPIFFS mount failed, error=%d\n"), result);
            _error = Error::Code::MountFailed;
        }
        return false;
    }
    if (!mounted()) {
        system()->printf(ROMSTR("ERROR: SPIFFS filesystem failed to mount\n"));
        _error = Error::Code::MountFailed;
        return false;
    }

    system()->printf(ROMSTR("Checking file system...\n"));
    result = SPIFFS_check(&_spiffsFileSystem);
    if (result != SPIFFS_OK) {
        system()->printf(ROMSTR("ERROR: Consistency check failed during SPIFFS mount, error=%d\n"), result);
        _error = Error::Code::InternalError;
        return false;
    }
    
    // Make sure there is a root directory
    spiffs_file rootDir = SPIFFS_open(SpiffsFS::sharedSpiffs(), "/", SPIFFS_CREAT, 0);
    if (rootDir < 0) {
        system()->printf(ROMSTR("ERROR: Could not create root directory, error=%d\n"), rootDir);
        _error = Error::Code::InternalError;
        return false;
    }
    
    SPIFFS_close(SpiffsFS::sharedSpiffs(), rootDir);

    system()->printf(ROMSTR("SPIFFS mounted successfully\n"));
    _error = Error::Code::OK;

    return true;
}

bool SpiffsFS::mounted() const
{
    return SPIFFS_mounted(const_cast<spiffs_t*>(&_spiffsFileSystem));
}

void SpiffsFS::unmount()
{
    if (mounted()) {
        SPIFFS_unmount(&_spiffsFileSystem);
    }
}

bool SpiffsFS::format()
{
    if (!mounted()) {
        internalMount();
    }
    unmount();
    
    int32_t result = SPIFFS_format(&_spiffsFileSystem);
    if (result != SPIFFS_OK) {
        system()->printf(ROMSTR("ERROR: SPIFFS format failed, error=%d\n"), result);
        return false;
    }
    mount();
    return true;
}

std::shared_ptr<SpiffsFile> SpiffsFS::rawOpen(const SpiffsDirectory::FileID& fileID, spiffs_flags flags, File::Type type, FileOpenMode mode)
{
    std::shared_ptr<SpiffsFile> file = std::shared_ptr<SpiffsFile>(new SpiffsFile(fileID.str().c_str(), flags));
    if (!file->valid()) {
        file->_file = SPIFFS_ERR_FILE_CLOSED;
        return file;
    }
    
    file->setType(type);
    file->_mode = mode;
    return file;
}

struct FileModeEntry {
    const char* _mode;
    spiffs_flags _flags;
};

std::shared_ptr<File> SpiffsFS::open(const char* name, FileOpenMode mode)
{
    // If our open mode allows file creation, tell find that it can create a file if it doesn't exist
    SpiffsDirectory::FindCreateMode createMode = SpiffsDirectory::FindCreateMode::File;
    if (mode == FileOpenMode::Read || mode == FileOpenMode::ReadUpdate) {
        createMode = SpiffsDirectory::FindCreateMode::None;
    }

    std::shared_ptr<File> file;
    SpiffsDirectory::FileID fileID;
    File::Type fileType;
    Error error;
    SpiffsDirectory::find(name, createMode, fileID, fileType, error);

    if (error == Error::Code::OK && fileType != File::Type::File) {
        error = Error::Code::NotAFile;
    } else if (error == Error::Code::OK || error == Error::Code::FileNotFound) {
        // If the file exists its contents are preserved unless it is in Write or
        // WriteAppend mode, in which case it's truncated. If it doesn't exist
        // use the SPIFFS_CREAT flag so the file is created and then opened for
        // read/write
        spiffs_flags flags = SPIFFS_RDWR;
        
        if (error == Error::Code::OK) {
            if (mode == FileOpenMode::Write || mode == FileOpenMode::WriteUpdate) {
                flags |= SPIFFS_TRUNC;
            }
        }
        if (mode == FileOpenMode::Read || mode == FileOpenMode::ReadUpdate) {
            error = Error::Code::FileNotFound;
        } else {
            flags |= SPIFFS_CREAT;
        }
        
        if (fileID) {
            file = SpiffsFS::rawOpen(fileID, flags, File::Type::File, mode);
        }
    }
    
    // At this point file is null because the file doesn't exist and the mode is Read
    // or ReadUpdate. Or it's null because there was an error, which is in 'error'.
    // Or we have a newly created file, an existing file whose contents have been
    // thrown away or an existing file whose contents have been preserved. In all
    // cases we're open read/write
    
    // If file is null, open a dummy file and put 'error' in it.
    if (!file) {
        assert(error != Error::Code::OK);
        file = SpiffsFS::rawOpen(SpiffsDirectory::FileID::bad(), 0, File::Type::File, mode);
        reinterpret_cast<SpiffsFile*>(file.get())->_error = error;
    }
    
    return file;
}

std::shared_ptr<Directory> SpiffsFS::openDirectory(const char* name)
{
    if (!mounted()) {
        return nullptr;
    }
    return std::shared_ptr<SpiffsDirectory>(new SpiffsDirectory(name));
}

bool SpiffsFS::makeDirectory(const char* name)
{
    SpiffsDirectory::FileID fileID;
    File::Type type;
    Error error;
    
    SpiffsDirectory::find(name, SpiffsDirectory::FindCreateMode::Directory, fileID, type, error);
    _error = error;
    return error == Error::Code::OK;
}

bool SpiffsFS::remove(const char* name)
{
    return SPIFFS_remove(&_spiffsFileSystem, name) == SPIFFS_OK;
}

bool SpiffsFS::rename(const char* src, const char* dst)
{
    return SPIFFS_rename(&_spiffsFileSystem, src, dst) == SPIFFS_OK;
}

uint32_t SpiffsFS::totalSize() const
{
    u32_t total, used;
    SPIFFS_info(const_cast<spiffs*>(&_spiffsFileSystem), &total, &used);
    return total;
}

uint32_t SpiffsFS::totalUsed() const
{
    u32_t total, used;
    SPIFFS_info(const_cast<spiffs*>(&_spiffsFileSystem), &total, &used);
    return used;
}

int32_t SpiffsFS::internalMount()
{
    return SPIFFS_mount(&_spiffsFileSystem, &_config, _spiffsWorkBuf,
                        _spiffsFileDescriptors, sizeof(_spiffsFileDescriptors), nullptr, 0, NULL);
}

Error::Code SpiffsFS::mapSpiffsError(spiffs_file spiffsError)
{
    assert(spiffsError < 0);

    switch(spiffsError) {
        case SPIFFS_ERR_NOT_MOUNTED         : return Error::Code::NotMounted;
        case SPIFFS_ERR_FULL                : return Error::Code::NoSpace;
        case SPIFFS_ERR_NOT_FOUND           : return Error::Code::FileNotFound;
        case SPIFFS_ERR_END_OF_OBJECT       : return Error::Code::ReadError;
        case SPIFFS_ERR_OUT_OF_FILE_DESCS   : return Error::Code::TooManyOpenFiles;
        case SPIFFS_ERR_NOT_WRITABLE        : return Error::Code::NotWritable;
        case SPIFFS_ERR_NOT_READABLE        : return Error::Code::NotReadable;
        case SPIFFS_ERR_MOUNTED             : return Error::Code::Mounted;
        case SPIFFS_ERR_FILE_EXISTS         : return Error::Code::Exists;
        case SPIFFS_ERR_NOT_CONFIGURED      : return Error::Code::NotMounted;
        default                             : return Error::Code::InternalError;
    }
}

SpiffsDirectory::SpiffsDirectory(const char* name)
{
    FileID fileID;
    File::Type fileType;
    if (!find(name, SpiffsDirectory::FindCreateMode::None, fileID, fileType, _error)) {
        if (_error == Error::Code::FileNotFound) {
            _error = Error::Code::DirectoryNotFound;
        } else if (fileType != File::Type::Directory) {
            _error = Error::Code::NotADirectory;
        }
        return;
    }
    
    _dirFile = SpiffsFS::rawOpen(fileID, SPIFFS_O_RDONLY, fileType);
    if (!_dirFile) {
        _error = Error::Code::InternalError;
        return;
    }

    next();
}

bool SpiffsDirectory::next()
{
    String s;
    
    if (_dirFile->eof()) {
        Entry entry;
        _dirFile->read(reinterpret_cast<char*>(&entry), sizeof(entry));
        if (entry.type() == EntryType::File ||
            entry.type() == EntryType::Directory) {
            uint8_t size = entry.size();
            s.reserve(size + 1);
            for ( ; size > 0; --size) {
                char c;
                _dirFile->read(&c, 1);
                s += c;
            }
            
            _dirFile->read(_fileID.value(), sizeof(_fileID));
            return true;
        }
    }
    
    return false;
}

bool SpiffsDirectory::find(const char* name, FindCreateMode createMode, FileID& fileID, File::Type& type, Error& error)
{
    error = Error::Code::OK;

    if (!name || name[0] != '/') {
        error = Error::Code::InvalidFileName;
        return false;
    }
    
    // Split up the name and find each component
    std::shared_ptr<File> file = SpiffsFS::rawOpen(FileID::root(), SPIFFS_O_RDWR, File::Type::Directory, FS::FileOpenMode::ReadUpdate);
    if (!file || !file->valid()) {
        error = Error::Code::InternalError;
        return false;
    }
    std::vector<String> components = String(name).split("/");
    
    for (int i = 0; i < components.size(); ++i) {
        if (components[i].empty()) {
            continue;
        }
        
        bool last = i == components.size() - 1;
        
        if (!findNameInDirectory(file, components[i], fileID, type)) {
            if (!last && createMode == FindCreateMode::Directory) {
                createEntry(file, components[i], File::Type::Directory, fileID);
                if (!findNameInDirectory(file, components[i], fileID, type)) {
                    // We should find the dir if we just created it
                    error = Error::Code::InternalError;
                }
            } else {
                error = last ? Error::Code::FileNotFound : Error::Code::DirectoryNotFound;
                
                // If the error is FileNotFound, the baseName doesn't exist. If we are createMode
                // is None, just return the error. Otherwise create a file or directory
                if (error == Error::Code::FileNotFound) {
                    if (createMode == FindCreateMode::None) {
                        return false;
                    }
                    type = (createMode == FindCreateMode::File) ? File::Type::File : File::Type::Directory;
                    createEntry(file, components[i], type, fileID);
                    error = Error::Code::OK;
                    return true;
                }
                return false;
            }
        }
        
        if (last) {
            return true;
        }
        
        if (type != File::Type::Directory) {
            error = Error::Code::DirectoryNotFound;
            return false;
        }
        
        file = SpiffsFS::rawOpen(fileID, SPIFFS_O_RDWR, type, FS::FileOpenMode::ReadUpdate);
    }

    error = Error::Code::DirectoryNotFound;
    return false;
}

bool SpiffsDirectory::findNameInDirectory(const std::shared_ptr<File>& dir, const String& name, FileID& fileID, File::Type& type)
{
    String s;
    
    while (!dir->eof()) {
        Entry entry;
        dir->read(reinterpret_cast<char*>(&entry), sizeof(entry));
        if (entry.type() == EntryType::File ||
            entry.type() == EntryType::Directory) {
            uint8_t size = entry.size();
            s.clear();
            s.reserve(size + 1);
            for ( ; size > 0; --size) {
                char c;
                dir->read(&c, 1);
                s += c;
            }
            
            dir->read(fileID.value(), sizeof(fileID));
            if (s == name) {
                type = (entry.type() == EntryType::File) ? File::Type::File : File::Type::Directory;
                return true;
            }
        }
    }
    
    return false;
}

void SpiffsDirectory::createEntry(const std::shared_ptr<File>& dir, const String& name, File::Type type, FileID& fileID)
{
    fileID = FileID::random();
    EntryType entryType = (type == File::Type::Directory) ? EntryType::Directory : EntryType::File;
    Entry entry(name.size(), entryType);
    dir->write(entry.value());
    dir->write(name.c_str(), static_cast<uint32_t>(name.size()));
    dir->write(fileID.value(), FileIDLength);
}

SpiffsDirectory::FileID SpiffsDirectory::FileID::random()
{
    SpiffsDirectory::FileID fileID;
    
    char offset = 0x21;
    char range = 0x7e - offset + 1;
    for (char i = 0; i < FileIDLength; ++i) {
        fileID._value[i] = static_cast<char>(rand() % range) + offset;
    }
    
    return fileID;
}

SpiffsFile::SpiffsFile(const char* name, spiffs_flags flags)
{
    if (!name || name[0] == '\0') {
        // We're opening a dummy file just so it can hold an error
        return;
    }
    _file = SPIFFS_open(SpiffsFS::sharedSpiffs(), name, flags, 0);
    _error = (_file < 0) ? SpiffsFS::mapSpiffsError(_file) : Error::Code::OK;
}

void SpiffsFile:: close()
{
    SPIFFS_close(SpiffsFS::sharedSpiffs(), _file);
    _error = Error::Code::FileClosed;
}

int32_t SpiffsFile::read(char* buf, uint32_t size)
{
    if (_mode == FS::FileOpenMode::Write || _mode == FS::FileOpenMode::Append) {
        _error = Error::Code::NotReadable;
        return -1;
    }
    return SPIFFS_read(SpiffsFS::sharedSpiffs(), _file, buf, size);
}

int32_t SpiffsFile::write(const char* buf, uint32_t size)
{
    if (_mode == FS::FileOpenMode::Read) {
        _error = Error::Code::NotWritable;
        return -1;
    }
    
    if (_mode == FS::FileOpenMode::AppendUpdate) {
        seek(0, SeekWhence::End);
    }

    return SPIFFS_write(SpiffsFS::sharedSpiffs(), _file, const_cast<char*>(buf), size);
}

bool SpiffsFile::seek(int32_t offset, SeekWhence whence)
{
    if (_mode == FS::FileOpenMode::Append) {
        _error = Error::Code::SeekNotAllowed;
        return -1;
    }

    int whenceFlag = SPIFFS_SEEK_SET;
    if (whence == SeekWhence::Cur) {
        whenceFlag = SPIFFS_SEEK_CUR;
    } else if (whence == SeekWhence::End) {
        whenceFlag = SPIFFS_SEEK_END;
    }
    return SPIFFS_lseek(SpiffsFS::sharedSpiffs(), _file, offset, whenceFlag) == SPIFFS_OK;
}

int32_t SpiffsFile::tell() const
{
    return SPIFFS_tell(SpiffsFS::sharedSpiffs(), _file);
}

bool SpiffsFile::eof() const
{
    return SPIFFS_eof(SpiffsFS::sharedSpiffs(), _file) > 0;
}


