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
 * Writed on 2015/7/16
 * Author: JianYi, eyjian@qq.com or eyjian@gmail.com
 */
#ifndef MOOON_UTILS_AES_HELPER_H
#define MOOON_UTILS_AES_HELPER_H
#include "mooon/utils/exception.h"
UTILS_NAMESPACE_BEGIN

// 高级加密标准（Advanced Encryption Standard），
// 在密码学中又称Rijndael加密法，是美国联邦政府采用的一种区块加密标准，用来替代DES
class CAESHelper
{
public:
    // 加密数据块分组长度，必须为128比特（密钥长度可以是128比特、192比特、256比特中的任意一个）
    static int aes_block_size;

public:
    // key 密钥
    //
    // 因为AES要求key长度只能为128或192或256比特中的一种，即16字节或24字节或32字节中的一种，
    // 当key的长度不足16字节时，CAESHelper自动补0足16字节，
    // 当key的长度间于16字节和24字节时，CAESHelper自动补0足24字节，
    // 当key的长度间于24字节和32字节时，CAESHelper自动补0足32字节，
    // 当key的长度超出32字节时，CAESHelper自动截取前32字节作为密钥
    CAESHelper(const std::string& key);
    ~CAESHelper();

    void encrypt(const std::string& in, std::string* out);
    void decrypt(const std::string& in, std::string* out);

private:
    // flag 为true表示加密，为false表示解密
    void aes(bool flag, const std::string& in, std::string* out, void* aes_key);

private:
    void* _encrypt_key;
    void* _decrypt_key;
    std::string _key;
};

UTILS_NAMESPACE_END
#endif // MOOON_UTILS_AES_HELPER_H
