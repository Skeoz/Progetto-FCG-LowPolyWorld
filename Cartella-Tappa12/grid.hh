#ifndef _GRID_HH
#define _GRID_HH



#include "debug.h"

#include <cmath>
#include <vector>
#include <limits>
#include <type_traits>
#include <cstring>


template<typename T>
constexpr auto lowest() {
    static_assert (std::is_arithmetic_v<T>, "Type must be arithmetic");
    return std::numeric_limits<T>::lowest ();
    // return constexpr (std::is_arithmetic_v<T>) ?
    //     std::numeric_limits<T>::lowest() : T{};
}

template<typename T>
constexpr auto highest() {
    static_assert (std::is_arithmetic_v<T>, "Type must be arithmetic");
    //if constexpr (std::is_arithmetic_v<T>) {
    return (std::is_floating_point_v<T>) ?
        std::numeric_limits<T>::infinity() :
        std::numeric_limits<T>::max();
    // } else {
    //     return T{};
    // }
}



template <class T>
class Pair
{
public:
    T x, y;

    Pair () : x (T ()), y (T ()) {}
    explicit Pair (const T& x, const T& y) : x (x), y (y) {}
    Pair (const Pair<T>& p) : x (p.x), y (p.y) {}

    Pair& operator= (const Pair& rhs)
    {
        x = rhs.x;
        y = rhs.y;
        return *this;
    }

    // logic operators

    bool operator< (const Pair& rhs) const
    {
        return x == rhs.x ? y < rhs.y : x < rhs.x;
    }

    bool operator> (const Pair& rhs) const
    {
        return x == rhs.x ? y > rhs.y : x > rhs.x;
    }

    bool operator<= (const Pair& rhs) const
    {
        return x == rhs.x ? y <= rhs.y : x <= rhs.x;
    }

    bool operator>= (const Pair& rhs) const
    {
        return x == rhs.x ? y >= rhs.y : x >= rhs.x;
    }

    bool operator== (const Pair& rhs) const
    {
        return x == rhs.x && y == rhs.y;
    }

    bool operator!= (const Pair& rhs) const
    {
        return !(*this == rhs);
    }

    // +,-,*,/ a Pair (with op= variant)

    Pair operator+ (const Pair& rhs) const
    {
        return Pair (x + rhs.x, y + rhs.y);
    }

    Pair operator- (const Pair& rhs) const
    {
        return Pair (x - rhs.x, y - rhs.y);
    }

    Pair& operator+= (const Pair& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    Pair& operator-= (const Pair& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    Pair operator* (const Pair& rhs) const
    {
        return Pair (x * rhs.x, y * rhs.y);
    }

    Pair operator/ (const Pair& rhs) const
    {
        return Pair (x / rhs.x, y / rhs.y);
    }

    Pair& operator*= (const Pair& rhs)
    {
        x *= rhs.x;
        y *= rhs.y;
        return *this;
    }

    Pair& operator/= (const Pair& rhs)
    {
        x /= rhs.x;
        y /= rhs.y;
        return *this;
    }

    // +,-,*,/ a scalar (with op= variant)

    Pair operator+ (const T rhs) const
    {
        return Pair (x + rhs, y + rhs);
    }

    Pair operator- (const T rhs) const
    {
        return Pair (x - rhs, y - rhs);
    }

    Pair& operator+= (const T rhs)
    {
        x += rhs;
        y += rhs;
        return *this;
    }

    Pair& operator-= (const T rhs)
    {
        x -= rhs;
        y -= rhs;
        return *this;
    }

    Pair operator* (const T rhs) const
    {
        return Pair (x * rhs, y * rhs);
    }

    Pair operator/ (const T rhs) const
    {
        return Pair (x / rhs, y / rhs);
    }

    Pair& operator*= (const T rhs)
    {
        x *= rhs;
        y *= rhs;
        return *this;
    }

    Pair& operator/= (const T rhs)
    {
        x /= rhs;
        y /= rhs;
        return *this;
    }

    // extents and bounding boxes checks

    bool is_border (const T& w, const T& h) const
    {
        return x == 0 || x == w - 1 || y == 0 || y == h - 1;
    }

    bool is_outer_border (const T& w, const T& h) const
    {
        return x == -1 || x == w || y == -1 || y == h;
    }

    bool is_inside (const T& w, const T& h) const
    {
        return x >= 0 && x < w && y >= 0 && y < h;
    }

    bool is_inside (const T& w, const T& h, double cut) const
    {
        double wc = w * (cut / 2.0);
        double hc = h * (cut / 2.0);
        return x >= 0 + wc && x < w - wc && y >= 0 + hc && y < h - hc;
    }

    bool is_inside (const T& w, const T& h, int window) const
    {
        return x >= window && x < w - window && y >= window && y < h - window;
    }

    bool is_inside (const Pair<T>& llc, const Pair<T>& hrc) const
    {
        return x >= llc.x && x < hrc.x && y >= llc.y && y < hrc.y;
    }

    bool is_outside (const T& w, const T& h) const
    {
        return !is_inside (w, h);
    }

    // limits

    static Pair lowest ()
    {
        const T bogus = ::lowest<T> ();
        return Pair (bogus, bogus);
    }

    static Pair highest ()
    {
        const T bogus = ::highest<T> ();
        return Pair (bogus, bogus);
    }
};




typedef Pair<double> Point;

inline double point_distance (const Point& a, const Point& b)
{
    return sqrt (((a.x - b.x) * (a.x - b.x)) + ((a.y - b.y) * (a.y - b.y)));
}



typedef Pair<int> Coord;

inline Coord point2coord (const Point& p)
{
    return Coord (floor (p.x), floor (p.y));
}

inline Point coord2point (const Coord& c)
{
    return Point ((double) c.x, (double) c.y);
}

inline unsigned coord2index (const Coord& c, const unsigned width)
{
    return (c.y * width) + c.x;
}
inline int coord2index_i (const Coord& c, const unsigned width)
{
    return (c.y * width) + c.x;
}

inline Coord index2coord(const int i, const int width)
{
    return Coord {i % width, i / width};
}


inline Point interpolate(Point a, Point b, double v)
{
    return Point(
        a.x + (b.x - a.x) * v,
        a.y + (b.y - a.y) * v
    );
}

inline Point interpolate(Coord a, Coord b, double v)
{
    return interpolate(coord2point(a), coord2point(b), v);
}



template <class T> class Grid
{
public:

    enum AccessType {ABYSS, PLATEU, BOUND, SPECMAX};

    int width;
    int height;
    std::vector<T> data;
    T bogus;

    Grid () : width (0), height (0), data () {}

    Grid (int width, int height) :
        width (width), height (height), data ()
    {
        resize (width, height);
    }

    Grid (int width, int height, T t) :
        width (width), height (height), data ()
    {
        resize (width, height);

        for (int i = 0; i < width; ++i)
            for (int j = 0; j < height; ++j)
                operator() (i, j) = t;
    }

    Grid (Grid& grid) :
        width (grid.width), height (grid.height), data ()
    {
        resize (width, height);

        for (int i = 0; i < width; ++i)
            for (int j = 0; j < height; ++j)
                operator() (i, j) = grid (i, j);
    }

    bool is_inside (const Coord& c)
    {
        return c.is_inside (width, height);
    }
    bool is_inside (const int x, const int y)
    {
        return is_inside (Coord (x, y));
    }

    bool is_outside (const Coord& c)
    {
        return c.is_outside (width, height);
    }
    bool is_outside (const int x, const int y)
    {
        return is_outside (Coord (x, y));
    }

    T& operator() (const Coord& c)
    {
        return (*this)(c, BOUND);
    }

    T& operator() (const Coord& c, const AccessType a)
    {
        if (c.is_inside (width, height))
            return data[coord2index (c, width)];
 
        switch (a)
        {
        case BOUND:
            print_fatal (-5, "Out of mem access with AccessType==BOUND, %d %d\n", c.x, c.y);
            break;

        case ABYSS:
            return bogus = lowest<T> ();
            break;

        case PLATEU:
            return data[coord2index (Coord (c.x<0? 0 : width-1,
                                            c.y<0? 0 : height-1),
                                     width)];
            break;

        case SPECMAX:
            return bogus = highest<T> ();
            break;

        default:
            ;
        }

        print_fatal (-5, "Impossible case, %d %d\n", c.x, c.y);
        return bogus = lowest<T> ();
    }

    T& operator() (const int x, const int y, const AccessType a = BOUND)
    {
        return (*this)(Coord (x, y), a);
    }

    unsigned size () const
    {
        return data.size();
    }

    void resize (const int w, const int h)
    {
        if (w < 0 || h < 0)
            print_fatal (-5, "Negative width or height: %d,%d.\n", w, h);

        data.resize (w * h);
        height = h;
        width = w;
    }

    void reset (T v)
    {
        memset (&data[0], v, data.size() * sizeof (data[0]));
    }
};


// TODO: Grid should be a NumericGrid that specializes this Grid
// TODO: Grid should have also the implementation of operator[] for access with coords and a default access mode (also, make different grid types with different default access modes)
template <class T> class SimpleGrid
{
public:
    int width;
    int height;
    std::vector<T> data;

    SimpleGrid () : width (0), height (0), data () {}

    SimpleGrid (int width, int height) :
        width (width), height (height), data ()
    {
        resize (width, height);
    }

    SimpleGrid (int width, int height, T t) :
        width (width), height (height), data ()
    {
        resize (width, height);

        for (int i = 0; i < width; ++i)
            for (int j = 0; j < height; ++j)
                operator () (i, j) = t;
    }

    T& operator() (const Coord& c)
    {
        if (c.is_inside (width, height))
            return data[coord2index (c, width)];
        print_fatal (-5, "Out of mem access SimpleGrid, %d %d\n", c.x, c.y);
    }

    T& operator() (const int x, const int y)
    {
        return (*this)(Coord (x, y));
    }

    unsigned size () const
    {
        return data.size();
    }

    void resize (const int w, const int h)
    {
        if (w < 0 || h < 0)
            print_fatal (-5, "Negative width or height: %d,%d.\n", w, h);

        data.resize (w * h);
        height = h;
        width = w;
    }

    void reset (T v)
    {
        memset (&data[0], v, data.size() * sizeof (data[0]));
    }
};



#endif
