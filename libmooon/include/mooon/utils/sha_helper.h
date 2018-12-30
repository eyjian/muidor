/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: jian yi, eyjian@qq.com or eyjian@gmail.com
 */
#ifndef MOOON_UTILS_SHA_HELPER_H
#define MOOON_UTILS_SHA_HELPER_H
#include "mooon/utils/string_utils.h"
#include <stdarg.h>
#include <stdint.h>
UTILS_NAMESPACE_BEGIN

enum SHAType
{
    SHA0   = 0, // Invalid
    SHA1   = 1,
    SHA224 = 224,
    SHA256 = 256,
    SHA384 = 384,
    SHA512 = 512
};

SHAType int2shatype(int n);

class CSHAHelper
{
public:
    static std::string sha(SHAType sha_type, const char* format, ...) __attribute__((format(printf, 2, 3)));
    static std::string lowercase_sha(SHAType sha_type, const char* format, ...) __attribute__((format(printf, 2, 3)));
    static std::string uppercase_sha(SHAType sha_type, const char* format, ...) __attribute__((format(printf, 2, 3)));

public:
    CSHAHelper(SHAType sha_type);
    ~CSHAHelper();
    SHAType sha_type() const { return _sha_type; }

    void update(const char* str, size_t size);
    void update(const std::string& str);

public:
    void to_string(std::string* str, bool uppercase=true) const;
    std::string to_string(bool uppercase=true) const;

private:
    SHAType _sha_type;
    void* _sha_context;
};

UTILS_NAMESPACE_END
#endif // MOOON_UTILS_SHA_HELPER_H
