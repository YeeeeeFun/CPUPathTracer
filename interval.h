// interval.h
#ifndef INTERVAL_H
#define INTERVAL_H

#include "constant.h"

class interval {
  public:
    double min, max;

    interval() : min(+infinity), max(-infinity) {} // 默认构造为空区间

    interval(double _min, double _max) : min(_min), max(_max) {}

    interval(const interval& a, const interval& b)
      : min(fmin(a.min, b.min)), max(fmax(a.max, b.max)) {}
    
    bool contains(double x) const {
        return min <= x && x <= max;
    }

    bool surrounds(double x) const {
        return min < x && x < max;
    }

    double clamp(double x) const {
        if (x < min) return min;
        if (x > max) return max;
        return x;
    }

    double size() const {
        return max - min;
    }

    interval expand(double delta) const {
        auto padding = delta/2;
        return interval(min - padding, max + padding);
    }

    static const interval empty, universe;
};

// 将静态成员定义为内联变量（C++17），这样定义就可以出现在头文件中，而不会违反一次定义规则（ODR）。
inline const interval interval::empty   (+infinity, -infinity);
inline const interval interval::universe(-infinity, +infinity);

inline interval operator+(const interval& ival, double displacement) {
    return interval(ival.min + displacement, ival.max + displacement);
}

inline interval operator+(double displacement, const interval& ival) {
    return ival + displacement;
}




#endif
