// Pnm.cpp — deterministic PGM/PPM I/O.
#include "Pnm.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

namespace anvil {
namespace {
bool writeAll(const std::string& path, const std::vector<uint8_t>& bytes) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const size_t n = std::fwrite(bytes.data(), 1, bytes.size(), f);
    const bool writeOk = n == bytes.size() && std::ferror(f) == 0;
    const bool closeOk = std::fclose(f) == 0;
    return writeOk && closeOk;
}
}

bool writePgm(std::string_view path, int width, int height,
              const uint8_t* data, size_t stride, int bitDepth) {
    if (width <= 0 || height <= 0 || !data || bitDepth < 1 || bitDepth > 16)
        return false;
    const size_t bps = bitDepth > 8 ? 2u : 1u;
    if (stride < static_cast<size_t>(width) * bps) return false;
    const uint32_t maxval = bitDepth == 16 ? 65535u : ((1u << bitDepth) - 1u);
    std::string header = "P5\n" + std::to_string(width) + " "
                       + std::to_string(height) + "\n"
                       + std::to_string(maxval) + "\n";
    std::vector<uint8_t> bytes(header.begin(), header.end());
    bytes.reserve(bytes.size() + static_cast<size_t>(width) * height * bps);
    for (int y=0; y<height; ++y) {
        const uint8_t* row = data + static_cast<size_t>(y) * stride;
        if (bps == 1) {
            bytes.insert(bytes.end(), row, row + width);
        } else {
            for (int x=0; x<width; ++x) {
                const size_t o=static_cast<size_t>(x)*2;
                uint32_t v=uint32_t(row[o]) | (uint32_t(row[o+1])<<8);
                v=std::min(v,maxval);
                bytes.push_back(static_cast<uint8_t>((v>>8)&0xff));
                bytes.push_back(static_cast<uint8_t>(v&0xff));
            }
        }
    }
    return writeAll(std::string(path),bytes);
}

bool writePpm(std::string_view path, int width, int height,
              const uint8_t* rgb, size_t stride) {
    if (width <= 0 || height <= 0 || !rgb || stride < static_cast<size_t>(width)*3)
        return false;
    std::string header="P6\n"+std::to_string(width)+" "+std::to_string(height)+"\n255\n";
    std::vector<uint8_t> bytes(header.begin(),header.end());
    for(int y=0;y<height;++y)
        bytes.insert(bytes.end(),rgb+static_cast<size_t>(y)*stride,
                     rgb+static_cast<size_t>(y)*stride+static_cast<size_t>(width)*3);
    return writeAll(std::string(path),bytes);
}

bool readPgm(std::string_view path, int& width, int& height,
             std::vector<uint8_t>& pixels, int* maxvalOut) {
    FILE* f=std::fopen(std::string(path).c_str(),"rb");
    if(!f)return false;
    int w=0,h=0,maxval=0;
    if(std::fscanf(f,"P5 %d %d %d",&w,&h,&maxval)!=3||w<=0||h<=0||maxval<=0||maxval>255){
        std::fclose(f);return false;
    }
    const int sep=std::fgetc(f);
    if(sep==EOF||!std::isspace(static_cast<unsigned char>(sep))){std::fclose(f);return false;}
    if(sep=='\r'){
        const int next=std::fgetc(f);
        if(next!='\n'&&next!=EOF)std::ungetc(next,f);
    }
    pixels.resize(static_cast<size_t>(w)*h);
    const size_t n=std::fread(pixels.data(),1,pixels.size(),f);
    const int trailing=std::fgetc(f);
    const bool ioError=std::ferror(f)!=0;
    std::fclose(f);
    if(n!=pixels.size()||trailing!=EOF||ioError)return false;
    width=w;height=h;
    if(maxvalOut)*maxvalOut=maxval;
    return true;
}
} // namespace anvil
