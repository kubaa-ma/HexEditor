#include "file.hpp"

#include <fstream>
#include <iostream>


bool FileData::load_file(const std::string& path){
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file) {
        std::cerr << "Error opening the file: " << path << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer.resize((size_t)size);

    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "Error opening the file: " << path << std::endl;
        buffer.clear();
        return false;
    }

    filepath = path;
    return true;
}

bool FileData::save_file(){
    if (filepath.empty()) {
        std::cerr << "No way save the file, use 'Save as'" << std::endl;
        return false;
    }

    return save_file(filepath);
}

bool FileData::save_file(const std::string& path){
    std::ofstream file(path, std::ios::binary);

    if (!file) {
        std::cerr << "Error opening the file for write: " << path << std::endl;
        return false;
    }

    if (!file.write(reinterpret_cast<const char*>(buffer.data()), (std::streamsize)buffer.size())) {
        std::cerr << "Error writing to file: " << path << std::endl;
        return false;
    }

    filepath = path;
    return true;
}

const std::vector<uint8_t>& FileData::get_buffer() const{
    return buffer;
}

std::vector<uint8_t>& FileData::get_mutable_buffer(){
    return buffer;
}

const std::string& FileData::get_filepath() const{
    return filepath;
}