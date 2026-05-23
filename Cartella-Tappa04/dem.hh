#ifndef _DEM_HH
#define _DEM_HH



#include "grid.hh"



class ASC_Header {
public:

    ASC_Header ();
    ASC_Header (const ASC_Header& asch);
    ASC_Header (const ASC_Header& asch, const Coord& crop_llc, const Coord& crop_hrc);
    ASC_Header (const char* name);
    ASC_Header (FILE* fp);
    ASC_Header (int width, int height, double cellsize, double xllcorner, double yllcorner, double nodata_value);

    void asc2img (const Point& a, Point* i) const;
    void img2asc (const Point& i, Point* a) const;

    Point asc2img (const Point& a) const;
    Point img2asc (const Point& i) const;

    int width, height;
    double xllcorner, yllcorner;
    double cellsize;
    double nodata_value;

private:
    void read_header (FILE* fp);
};

class ASC_Reader {
public:
    ASC_Header header;
    double* data;

    ASC_Reader (FILE* fp, const ASC_Header& h);
    ~ASC_Reader ();
    double get_pixel (unsigned int x, unsigned int y) const;
};


class Dem : public Grid<double> {

public:
    ASC_Header header;
    double min, max;

    Dem ();
    Dem (Dem& dem);
    Dem (const int w, const int h);
    Dem (const ASC_Header header);
    Dem (const char* filename);

    bool is_equal (const Coord& a, const Coord& b);
    void quantize (double quant);
    void scale (double scalefactor);

    void write (const char* filename, bool round2int = false);

    double& operator() (const Coord& c);
    double& operator() (const Coord& c, const AccessType a);
    double& operator() (const int x, const int y, const AccessType a = BOUND);

    void set_minmax ();
    void print_info (const char *filename = 0) const;
};

#endif
