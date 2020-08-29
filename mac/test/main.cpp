/*-------------------------------------------------------------------------
    This source file is a part of m8rscript
    For the latest info, see http:www.marrin.org/
    Copyright (c) 2018-2019, Chris Marrin
    All rights reserved.
    Use of this source code is governed by the MIT license that can be
    found in the LICENSE file.
-------------------------------------------------------------------------*/

#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <stdio.h>

#include "Application.h"
#include "MacSystemInterface.h"
#include "Marly.h"
#include "MFS.h"

marly::MarlyScriptingLanguage marlyScriptingLanguage;

const char* fileList[] = {
    "scripts/examples/TimeZoneDBClient.marly",
    "scripts/timing/timing.marly",
    "scripts/timing/timing.marly",
    "scripts/simple/hello.marly",
    "scripts/NetworkTime.marly"
};

int main(int argc, char * argv[])
{
    m8r::initMacSystemInterface("m8rFSFile", [](const char* s) { ::printf("%s", s); });
    m8r::Application application(23);
    
    m8r::system()->registerScriptingLanguage(&marlyScriptingLanguage);
    
    // Upload files if present
    int count = sizeof(fileList) / sizeof(const char*);
    const char* uploadPath = "/sys/bin";
    
    for (int i = 0; i < count; ++i) {
        const char* uploadFilename = fileList[i];

        m8r::String toPath;
        FILE* fromFile = fopen(uploadFilename, "r");
        if (!fromFile) {
            fprintf(stderr, "Unable to open '%s' for upload, skipping\n", uploadFilename);
        } else if (m8r::system()->fileSystem()) {
            m8r::Vector<m8r::String> parts = m8r::String(uploadFilename).split("/");
            m8r::String baseName = parts[parts.size() - 1];
            
            if (uploadPath[0] != '/') {
                toPath += '/';
            }
            toPath += uploadPath;
            if (toPath[toPath.size() - 1] != '/') {
                toPath += '/';
            }
            
            // Make sure the directory path exists
            m8r::system()->fileSystem()->makeDirectory(toPath.c_str());
            if (m8r::system()->fileSystem()->lastError() != m8r::Error::Code::OK) {
                m8r::system()->print(m8r::Error::formatError(m8r::system()->fileSystem()->lastError().code(), 
                                                        "Unable to create '%s'", toPath.c_str()).c_str());
            } else {
                toPath += baseName;
                
                m8r::Mad<m8r::File> toFile(m8r::system()->fileSystem()->open(toPath.c_str(), m8r::FS::FileOpenMode::Write));
                if (!toFile->valid()) {
                    m8r::system()->print(m8r::Error::formatError(toFile->error().code(), 
                                                            "Error: unable to open '%s'", toPath.c_str()).c_str());
                } else {
                    bool success = true;
                    while (1) {
                        char c;
                        size_t size = fread(&c, 1, 1, fromFile);
                        if (size != 1) {
                            if (!feof(fromFile)) {
                                fprintf(stderr, "Error reading '%s', upload failed\n", uploadFilename);
                                success = false;
                            }
                            break;
                        }
                        
                        toFile->write(c);
                        if (!toFile->valid()) {
                            fprintf(stderr, "Error writing '%s', upload failed\n", toPath.c_str());
                            success = false;
                            break;
                        }
                    }
                    toFile->close();
                    if (success) {
                        printf("Uploaded '%s' to '%s'\n", uploadFilename, toPath.c_str());
                    }
                }
            }
        }
        fclose(fromFile);
    }

    application.runAutostartTask("/sys/bin/hello.marly");
    
    while (true) {
        application.runOneIteration();
    }

    return 0;
}
