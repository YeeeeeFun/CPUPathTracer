// rtw_stb_image.h
#ifndef RTW_STB_IMAGE_H
#define RTW_STB_IMAGE_H

// 在 Microsoft Visual C++ 中关闭该头文件的严格警告。
#ifdef _MSC_VER
    #pragma warning (push, 0)
#endif

#define STBI_FAILURE_USERMSG
#include "external/stb_image.h"

#include <cstdlib>
#include <iostream>

class rtw_image {
  public:
    rtw_image() : data(nullptr) {}

    rtw_image(const char* image_filename) {
        // 从指定路径加载图像。若设置 RTW_IMAGES，则只在该目录查找；否则从当前
        // 目录开始，逐级查找 images 子目录，最多向上查找六层。加载失败时宽高为 0。

        auto filename = std::string(image_filename);
        auto imagedir = getenv("RTW_IMAGES");

        // 在常用位置中查找图像文件。
        if (imagedir && load(std::string(imagedir) + "/" + image_filename)) return;

        std::string prefix;
        for (int depth = 0; depth <= 6; ++depth) {
            if (load(prefix + filename)) return;
            if (load(prefix + "images/" + filename)) return;
            prefix += "../";
        }

        std::cerr << "ERROR: Could not load image file '" << image_filename << "'.\n";
    }

    ~rtw_image() { stbi_image_free(data); }

    bool load(const std::string filename) {
        // 加载指定图像，成功时返回 true。
        auto n = bytes_per_pixel; // 接收原始图像的通道数
        data = stbi_load(filename.c_str(), &image_width, &image_height, &n, bytes_per_pixel);
        bytes_per_scanline = image_width * bytes_per_pixel;
        return data != nullptr;
    }

    int width()  const { return (data == nullptr) ? 0 : image_width; }
    int height() const { return (data == nullptr) ? 0 : image_height; }

    const unsigned char* pixel_data(int x, int y) const {
        // 返回像素 (x,y) 的三个字节；无数据时返回洋红色占位像素。
        static unsigned char magenta[] = { 255, 0, 255 };
        if (data == nullptr) return magenta;

        x = clamp(x, 0, image_width);
        y = clamp(y, 0, image_height);

        return data + y*bytes_per_scanline + x*bytes_per_pixel;
    }

  private:
    const int bytes_per_pixel = 3;
    unsigned char *data;
    int image_width, image_height;
    int bytes_per_scanline;

    static int clamp(int x, int low, int high) {
        // 将数值限制在 [low,high) 范围内。
        if (x < low) return low;
        if (x < high) return x;
        return high - 1;
    }
};

// 恢复 MSVC 编译器警告。
#ifdef _MSC_VER
    #pragma warning (pop)
#endif

#endif
