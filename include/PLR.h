//这里记录的是原本的PLR.h
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include <iostream>
struct Slope {
  using SX=__int128;
  using SY=__int128;
  SX dx{};
  SY dy{};  //会调用对应类型的默认构造函数，这里会将其置为0

  bool operator<(const Slope &p) const { return dy * p.dx < dx * p.dy; }
  bool operator>(const Slope &p) const { return dy * p.dx > dx * p.dy; }
  bool operator==(const Slope &p) const { return dy * p.dx == dx * p.dy; }
  bool operator!=(const Slope &p) const { return dy * p.dx != dx * p.dy; }
  explicit operator long double() const { return dy / (long double) dx; }//强制类型转换过程中自动调用
};
struct Point {
  using X=uint64_t;
  using Y=uint64_t;
  using SX=__int128;
  using SY=__int128;
  X x{};
  Y y{};

  Slope operator-(const Point &p) const { 
      Slope S;
      S.dx=SX(x)-p.x;
      S.dy=SY(y)-p.y;
      return S;}
      // return (Slope){SX(x) - p.x, SY(y) - p.y}; }  //将自己的x/y成员转为SX类型再进行运算，返回Slope类型变量
};

class CanonicalSegment {
  friend class PLR;
  using X=uint64_t;
  using Y=uint64_t;
  Point rectangle[4];
  X first;

  CanonicalSegment(const Point &p0, const Point &p1, X first) : rectangle{p0, p1, p0, p1}, first(first) {};

  CanonicalSegment(const Point (&rectangle)[4], X first)
    : rectangle{rectangle[0], rectangle[1], rectangle[2], rectangle[3]}, first(first) {};

  bool one_point() const {
    return rectangle[0].x == rectangle[2].x && rectangle[0].y == rectangle[2].y
        && rectangle[1].x == rectangle[3].x && rectangle[1].y == rectangle[3].y;
  }

public:
  CanonicalSegment() = default;

  X get_first_x() const { return first; }

  std::pair<long double, long double> get_intersection() const {
    auto &p0 = rectangle[0];
    auto &p1 = rectangle[1];
    auto &p2 = rectangle[2];
    auto &p3 = rectangle[3];
    auto slope1 = p2 - p0;
    auto slope2 = p3 - p1;

    if (one_point() || slope1 == slope2)
      return {p0.x, p0.y};

    auto p0p1 = p1 - p0;
    auto a = slope1.dx * slope2.dy - slope1.dy * slope2.dx;
    auto b = (p0p1.dx * slope2.dy - p0p1.dy * slope2.dx) / static_cast<long double>(a);
    auto i_x = p0.x + b * slope1.dx;
    auto i_y = p0.y + b * slope1.dy;
    return {i_x, i_y};
  }
  std::pair<long double, __int128> get_floating_point_segment(const uint64_t &origin) const {
    if (one_point())
      return {0, (rectangle[0].y + rectangle[1].y) / 2};

   // if (std::is_integral_v<uint64_t> && std::is_integral_v<uint64_t>) {

      auto slope = rectangle[3] - rectangle[1];
      auto intercept_n = slope.dy * (__int128(origin) - rectangle[1].x);
      auto intercept_d = slope.dx;
      auto rounding_term = ((intercept_n < 0) ^ (intercept_d < 0) ? -1 : +1) * intercept_d / 2;
      auto intercept = (intercept_n + rounding_term) / intercept_d + rectangle[1].y;
      return {static_cast<long double>(slope), intercept};
  }
  std::pair<long double, long double> get_slope_intercept() const {
    if (one_point())
      return {0, (rectangle[0].y + rectangle[1].y) / 2};

   // auto[i_x, i_y] = get_intersection();
    std::pair<long double, long double> p = get_intersection();
   // auto[min_slope, max_slope] = get_slope_range();
    std::pair<long double, long double> p2 = get_slope_range();
    auto slope = (p2.first + p2.second) / 2.;
    auto intercept = p.second - p.first * slope;
    return {slope, intercept};
  }
  std::pair<long double, long double> get_slope_range() const {
    if (one_point())
      return {0, 1};

    auto min_slope = static_cast<long double>(rectangle[2] - rectangle[0]);
    auto max_slope = static_cast<long double>(rectangle[3] - rectangle[1]);
    return {min_slope, max_slope};
  }




};


class PLR{
private:
  using SX = __int128;  
  using SY = __int128;
  using X = uint64_t;
  using Y = uint64_t;




  const Y epsilon;
  std::vector<Point> lower;
  std::vector<Point> upper;
  X first_x = 0;
  X last_x = 0;
  size_t lower_start = 0;
  size_t upper_start = 0;
  
  Point rectangle[4];

  auto cross(const Point &O, const Point &A, const Point &B) const {
    Slope OA = A - O;
    Slope OB = B - O;
    return OA.dx * OB.dy - OA.dy * OB.dx;
  }

public:
  size_t points_in_hull = 0;

  explicit PLR(Y epsilon) : epsilon(epsilon), lower(), upper() {
    if (epsilon < 0)
      throw std::invalid_argument("epsilon cannot be negative");
    upper.reserve(1u << 16);
    lower.reserve(1u << 16);
    //std::cout<<"this is PLR Model"<<std::endl;
  }

  bool add_point(const X &x, const Y &y) {
    if (points_in_hull > 0 && x <= last_x)
      throw std::logic_error("Points must be increasing by x.");

    last_x = x;
    auto max_y = std::numeric_limits<Y>::max();
    auto min_y = std::numeric_limits<Y>::lowest();
    Point p1;  //问题就在这里
    p1.x=x;
    if(y >= max_y - epsilon){
        p1.y=max_y;
    }else{
        p1.y=y+epsilon;
    }
    Point p2; 
    p2.x=x; 
    if(y <= min_y + epsilon){
        p2.y=min_y;
    }else{
        p2.y=y-epsilon;
    }     //问题就在这里
    
    if (points_in_hull == 0) {
      first_x = x;
      rectangle[0] = p1;
      rectangle[1] = p2;
      upper.clear();//清空容器内容，但是不会清空其容量
      lower.clear();
      upper.push_back(p1);
      lower.push_back(p2);
      upper_start = lower_start = 0;
      ++points_in_hull;
      return true;
    }

    if (points_in_hull == 1) {
      rectangle[2] = p2;
      rectangle[3] = p1;
      upper.push_back(p1);
      lower.push_back(p2);
      ++points_in_hull;
      return true;
    }
    auto slope1 = rectangle[2] - rectangle[0];
    auto slope2 = rectangle[3] - rectangle[1];
    bool outside_line1 = p1 - rectangle[2] < slope1;
    bool outside_line2 = p2 - rectangle[3] > slope2;

    if (outside_line1 || outside_line2) {
      points_in_hull = 0;
      return false;
    }

    if (p1 - rectangle[1] < slope2) {
      // Find extreme slope
      auto min = lower[lower_start] - p1;
      auto min_i = lower_start;
      for (auto i = lower_start + 1; i < lower.size(); i++) {
        auto val = lower[i] - p1;
        if (val > min)
          break;
        min = val;
        min_i = i;
      }

      rectangle[1] = lower[min_i];
      rectangle[3] = p1;
      lower_start = min_i;

      // Hull update
      auto end = upper.size();
      for (; end >= upper_start + 2 && cross(upper[end - 2], upper[end - 1], p1) <= 0; --end)
          continue;
      upper.resize(end);
      upper.push_back(p1);
    }

    if (p2 - rectangle[0] > slope1) {
      // Find extreme slope
      auto max = upper[upper_start] - p2;
      auto max_i = upper_start;
      for (auto i = upper_start + 1; i < upper.size(); i++) {
          auto val = upper[i] - p2;
          if (val < max)
              break;
          max = val;
          max_i = i;
      }

      rectangle[0] = upper[max_i];
      rectangle[2] = p2;
      upper_start = max_i;

      // Hull update
      auto end = lower.size();
      for (; end >= lower_start + 2 && cross(lower[end - 2], lower[end - 1], p2) >= 0; --end)
          continue;
      lower.resize(end);
      lower.push_back(p2);
    }

    ++points_in_hull;
    return true;
  }

  CanonicalSegment get_segment(){
    if (points_in_hull == 1)
      return CanonicalSegment(rectangle[0], rectangle[1], first_x);
    return CanonicalSegment(rectangle, first_x);
  }

  void reset() {
    points_in_hull = 0;
    lower.clear();
    upper.clear();
  }

};





