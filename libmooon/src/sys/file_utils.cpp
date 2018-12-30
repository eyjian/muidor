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
 * Author: jian yi, eyjian@qq.com
 */
#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "sys/file_utils.h"
#include "sys/close_helper.h"
#include "utils/md5_helper.h"
#include "utils/string_utils.h"
SYS_NAMESPACE_BEGIN

bool CFileUtils::md5sum(std::string* md5_str, int fd, const char* filepath) throw (CSyscallException)
{
    utils::CMd5Helper md5;
    char line[mooon::SIZE_4K];
    ssize_t file_size = 0;

    while (true)
    {
        const ssize_t n = read(fd, line, sizeof(line));

        if (0 == n)
        {
            break;
        }
        else if (-1 == n)
        {
            if (NULL == filepath)
                THROW_SYSCALL_EXCEPTION(NULL, errno, "read");
            else
                THROW_SYSCALL_EXCEPTION(
                        utils::CStringUtils::format_string("read file://%s failed: %s",
                                filepath, strerror(errno)),
                        errno, "read");
        }
        else
        {
            file_size += n;
            md5.update(line, n);
            if (n < static_cast<ssize_t>(sizeof(line)))
                break;
        }
    }

    *md5_str = md5.to_string();
    return file_size > 0;
}

bool CFileUtils::md5sum(std::string* md5_str, const char* filepath) throw (CSyscallException)
{
    const int fd = open(filepath, O_RDONLY);
    if (-1 == fd)
        THROW_SYSCALL_EXCEPTION(
                utils::CStringUtils::format_string("open file://%s failed: %s",
                        filepath, strerror(errno)),
                errno, "open");

    try
    {
        bool ret = md5sum(md5_str, fd, filepath);
        close(fd);
        return ret;
    }
    catch (...)
    {
        close(fd);
        throw;
    }
}

bool CFileUtils::compare(int fdA, int fdB) throw (CSyscallException)
{
    try
    {
        std::string fileA_md5, fileB_md5;
        const bool r1 = md5sum(&fileA_md5, fdA);
        const bool r2 = md5sum(&fileB_md5, fdB);
        return (!r1 && !r2) || (fileA_md5==fileB_md5);
    }
    catch (CSyscallException& ex)
    {
        if (ENOENT == ex.errcode())
            return false;
        throw;
    }
}

bool CFileUtils::compare(const char* fileA, const char* fileB) throw (CSyscallException)
{
    try
    {
        std::string fileA_md5, fileB_md5;
        const bool r1 = md5sum(&fileA_md5, fileA);
        const bool r2 = md5sum(&fileB_md5, fileB);
        return (!r1 && !r2) || (fileA_md5==fileB_md5);
    }
    catch (CSyscallException& ex)
    {
        if (ENOENT == ex.errcode())
            return false;
        throw;
    }
}

bool CFileUtils::exists(const char* filepath) throw (CSyscallException)
{
    if (0 == access(filepath, F_OK))
    {
        return true;
    }
    else
    {
        if (ENOENT == errno)
            return false;
        THROW_SYSCALL_EXCEPTION(
                utils::CStringUtils::format_string("access file://%s failed: %s",
                        filepath, strerror(errno)),
                errno, "access");
    }
}

size_t CFileUtils::file_copy(int src_fd, int dst_fd) throw (CSyscallException)
{
    char buf[IO_BUFFER_MAX];
    size_t file_size = 0;

    for (;;)
    {    
        ssize_t ret = read(src_fd, buf, sizeof(buf)-1);
        if (ret > 0)
        {      
            for (;;)
            {            
                if (write(dst_fd, buf, ret) != ret)
                {
                    if (EINTR == errno) continue;
                    THROW_SYSCALL_EXCEPTION(NULL, errno, "write");
                }

                break;
            }

            file_size += (size_t)ret;
        }
        else if (0 == ret)
        {
            break;
        }
        else
        {
            if (EINTR == errno) continue;
            THROW_SYSCALL_EXCEPTION(NULL, errno, "read");
        }
    }

    return file_size;
}

size_t CFileUtils::file_copy(int src_fd, const char* dst_filename) throw (CSyscallException)
{    
    int dst_fd = open(dst_filename, O_WRONLY|O_CREAT|O_EXCL);
    if (-1 == src_fd)
        THROW_SYSCALL_EXCEPTION(
                utils::CStringUtils::format_string("open file://%s failed: %s",
                        dst_filename, strerror(errno)),
                errno, "open");

    sys::CloseHelper<int> ch(dst_fd);
    return file_copy(src_fd, dst_fd);
}

size_t CFileUtils::file_copy(const char* src_filename, int dst_fd) throw (CSyscallException)
{
    int src_fd = open(src_filename, O_RDONLY);
    if (-1 == src_fd)
        THROW_SYSCALL_EXCEPTION(
                utils::CStringUtils::format_string("open file://%s failed: %s",
                        src_filename, strerror(errno)),
                errno, "open");

    sys::CloseHelper<int> ch(src_fd);
    return file_copy(src_fd, dst_fd);
}

size_t CFileUtils::file_copy(const char* src_filename, const char* dst_filename) throw (CSyscallException)
{ 
    int src_fd = open(src_filename, O_RDONLY);
    if (-1 == src_fd)
        THROW_SYSCALL_EXCEPTION(
                utils::CStringUtils::format_string("open file://%s failed: %s",
                        src_filename, strerror(errno)),
                errno, "open");

    sys::CloseHelper<int> src_ch(src_fd);
    int dst_fd = open(dst_filename, O_WRONLY|O_CREAT|O_EXCL);
    if (-1 == dst_fd)
        THROW_SYSCALL_EXCEPTION(
                utils::CStringUtils::format_string("open file://%s failed: %s",
                        dst_filename, strerror(errno)),
                errno, "open");

    sys::CloseHelper<int> dst_ch(dst_fd);
    return file_copy(src_fd, dst_fd);
}

off_t CFileUtils::get_file_size(int fd) throw (CSyscallException)
{
    struct stat buf;
    if (-1 == fstat(fd, &buf))
        THROW_SYSCALL_EXCEPTION(NULL, errno, "fstat");

    if (!S_ISREG(buf.st_mode))
        THROW_SYSCALL_EXCEPTION(NULL, errno, "fstat");

    return buf.st_size;
}

off_t CFileUtils::get_file_size(const char* filepath) throw (CSyscallException)
{
    int fd = open(filepath, O_RDONLY);
    if (-1 == fd)
        THROW_SYSCALL_EXCEPTION(
                utils::CStringUtils::format_string("open file://%s failed: %s",
                        filepath, strerror(errno)),
                errno, "open");

    sys::CloseHelper<int> ch(fd);
    return get_file_size(fd);
}

uint32_t CFileUtils::crc32_file(int fd) throw (CSyscallException)
{
    uint32_t crc = 0;
    int page_size = sys::CUtils::get_page_size();
    char* buffer = new char[page_size];
    utils::DeleteHelper<char> dh(buffer, true);

    if (-1 == lseek(fd, 0, SEEK_SET))
    {
        THROW_SYSCALL_EXCEPTION(NULL, errno, "lseek");
    }
    for (;;)
    {
        int retval = read(fd, buffer, page_size);
        if (0 == retval)
        {
            break;
        }
        if (-1 == retval)
        {
            THROW_SYSCALL_EXCEPTION(NULL, errno, "read");
        }

        crc = crc32(crc, (unsigned char*)buffer, retval);
        if (retval < page_size)
        {
            break;
        }
    }
    if (-1 == lseek(fd, 0, SEEK_SET))
    {
        THROW_SYSCALL_EXCEPTION(NULL, errno, "lseek");
    }
    
    return crc;
}

uint32_t CFileUtils::crc32_file(const char* filepath) throw (CSyscallException)
{
    int fd = open(filepath, O_RDONLY);
    if (-1 == fd)
        THROW_SYSCALL_EXCEPTION(
                utils::CStringUtils::format_string("open file://%s failed: %s",
                        filepath, strerror(errno)),
                errno, "open");

    sys::CloseHelper<int> ch(fd);
    return crc32_file(fd);
}

uint32_t CFileUtils::get_file_mode(int fd) throw (CSyscallException)
{
    struct stat st;
    if (-1 == fstat(fd, &st))
        THROW_SYSCALL_EXCEPTION(NULL, errno, "fstat");

    return st.st_mode;
}

void CFileUtils::remove(const char* filepath) throw (CSyscallException)
{
    if (-1 == unlink(filepath))
    {
        if (errno != ENOENT)
            THROW_SYSCALL_EXCEPTION(
                    utils::CStringUtils::format_string("unlink file://%s failed: %s",
                            filepath, strerror(errno)),
                    errno, "unlink");
    }
}

void CFileUtils::rename(const char* from_filepath, const char* to_filepath) throw (CSyscallException)
{
    if (-1 == ::rename(from_filepath, to_filepath))
        THROW_SYSCALL_EXCEPTION(
                utils::CStringUtils::format_string("rename file://%s to file://%s failed: %s",
                        from_filepath, to_filepath, strerror(errno)),
                errno, "rename");
}

SYS_NAMESPACE_END
