#include "album_palette.h"
#include <cassert>
#include <vector>
#include <cmath>
static double luminance(uint32_t rgb) {
    double result=0; const double weights[]={.0722,.7152,.2126};
    for(unsigned i=0;i<3;++i) { double v=((rgb>>(i*8))&255)/255.;
        result+=weights[i]*(v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4)); }
    return result;
}
int main() {
    const album_palette::Palette standard{};
    assert(album_palette::extract(nullptr,320,320)==standard);
    uint8_t one[]={0,0};
    assert(album_palette::extract(one,0,1)==standard);
    assert(album_palette::extract(one,1025,1)==standard);
    for(uint16_t value : {uint16_t(0),uint16_t(0xffff),uint16_t(0x8410)}) {
        uint8_t pixel[]={uint8_t(value),uint8_t(value>>8)};
        assert(album_palette::extract(pixel,1,1)==standard);
    }
    for(uint16_t value : {uint16_t(0xf800),uint16_t(0x07e0),uint16_t(0x001f),uint16_t(0xfd00)}) {
        std::vector<uint8_t> image(64*64*2);
        for(size_t i=0;i<image.size();i+=2) { image[i]=value&255; image[i+1]=value>>8; }
        const auto palette=album_palette::extract(image.data(),64,64);
        assert(palette!=standard);
        for(uint32_t background : {palette.background,palette.panel}) {
            assert((luminance(0xedf3e8)+.05)/(luminance(background)+.05)>=4.5);
            assert((luminance(palette.accent)+.05)/(luminance(background)+.05)>=4.5);
            assert((luminance(palette.search)+.05)/(luminance(background)+.05)>=4.5);
        }
        assert(album_palette::extract(image.data(),64,64)==palette);
    }
    // White margins must not overwhelm coloured artwork; tiny corner pixels
    // must not colour an otherwise monochrome cover.
    std::vector<uint8_t> image(32*32*2,255);
    image[0]=0; image[1]=0xf8;
    assert(album_palette::extract(image.data(),32,32)==standard);
    for(size_t i=0;i<image.size()/2;i+=2) { image[i]=0;image[i+1]=0xf8; }
    assert(album_palette::extract(image.data(),32,32)!=standard);
}
