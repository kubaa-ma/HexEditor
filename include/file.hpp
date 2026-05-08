#pragma once

#include <string>
#include <vector>
#include <cstdint>

class FileData {
private:
    std::vector<uint8_t> buffer;
    std::string          filepath;

public:
    bool load_file(const std::string& path);

    bool save_file();
    bool save_file(const std::string& path);

    const std::vector<uint8_t>& get_buffer()         const;
    std::vector<uint8_t>&       get_mutable_buffer();
    const std::string&          get_filepath()        const;

    bool is_loaded() const { return !buffer.empty(); }
};