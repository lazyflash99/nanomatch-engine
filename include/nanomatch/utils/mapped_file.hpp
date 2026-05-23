#pragma once

#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

namespace nanomatch {

class MappedFile {
public:
    MappedFile(const std::string& filename, int advice = MADV_SEQUENTIAL) {
        fd_ = open(filename.c_str(), O_RDONLY);
        if (fd_ == -1) throw std::runtime_error("open failed");

        struct stat sb;
        if (fstat(fd_, &sb) == -1) {
            close(fd_);
            throw std::runtime_error("fstat failed");
        }
        size_ = sb.st_size;

        data_ = static_cast<char*>(mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0));
        if (data_ == MAP_FAILED) {
            close(fd_);
            throw std::runtime_error("mmap failed");
        }
        madvise(data_, size_, advice);
    }

    ~MappedFile() {
        if (data_ != MAP_FAILED) munmap(data_, size_);
        if (fd_ != -1) close(fd_);
    }

    const char* data() const { return data_; }
    size_t size() const { return size_; }

private:
    int fd_ = -1;
    char* data_ = nullptr;
    size_t size_ = 0;
};

} // namespace nanomatch
