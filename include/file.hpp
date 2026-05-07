#pragma once
#define FILE_NAME "obrazek.png"

#include <stdint.h>
#include <vector>


class FileData{
private:
    std::vector<uint8_t> buffer;
public:
    bool get_data_from_file();
    
    const std::vector<uint8_t>& get_buffer() const;

};