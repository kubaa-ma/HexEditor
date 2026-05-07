#include "file.hpp"
#include <stdio.h>
#include <iostream>
#include <stdint.h>
#include <fstream>
#include <vector>



bool FileData::get_data_from_file(){
    std::ifstream file(FILE_NAME, std::ios::binary);

    if (!file){
        std::cout << "Error opening the file: "<< FILE_NAME << std::endl;
        return false;
    }

    char byte;

    while (file.read(&byte, 1)){
        buffer.push_back(static_cast<uint8_t>(byte));
    }

    return true;
}

const std::vector<uint8_t>& FileData::get_buffer() const{
    return buffer;
}

bool FileData::load_file(const std::string& path){
    std::ifstream file(path, std::ios::binary);

    if(!file){
        return false;
    }

    buffer.clear();
    char byte;
    while(file.read(&byte, 1)){
        buffer.push_back(static_cast<uint8_t>(byte));
    }
    return true;
}