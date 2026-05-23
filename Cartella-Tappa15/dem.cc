#include "dem.hh"

#include <stdlib.h>
#include <cctype>
#include <string>
#include <cstring>
using std::strerror;
using std::string;
using std::tolower;



ASC_Header::ASC_Header () :
    width (0), height (0), xllcorner (0), yllcorner (0),
    cellsize (0), nodata_value (0) {}

ASC_Header::ASC_Header (const ASC_Header& asch) :
    width (asch.width), height (asch.height),
    xllcorner (asch.xllcorner), yllcorner (asch.yllcorner),
    cellsize (asch.cellsize), nodata_value (asch.nodata_value) {}

ASC_Header::ASC_Header (const ASC_Header& asch, const Coord& crop_llc, const Coord& crop_hrc)
{
    width = crop_hrc.x - crop_llc.x;
    height = crop_hrc.y - crop_llc.y;
    Point llcorner = asch.img2asc (Point (crop_llc.x, crop_llc.y));
    xllcorner = llcorner.x;
    yllcorner = llcorner.y;
    cellsize = asch.cellsize;
    nodata_value = asch.nodata_value;
}

ASC_Header::ASC_Header (const char* name)
{
    FILE* fp;
    fp = fopen (name, "r");
    if (fp == NULL)
        print_fatal (2, "Could not open file `%s'. %s\n", name, strerror (errno));

    read_header (fp);

    fclose (fp);
}

ASC_Header::ASC_Header (FILE* fp)
{
    read_header (fp);
}

ASC_Header::ASC_Header (int w, int h, double cs, double xllc, double yllc, double nv) :
    width (w), height (h), xllcorner (xllc), yllcorner (yllc),
    cellsize (cs), nodata_value (nv) {}

void ASC_Header::asc2img (const Point& a, Point* i) const
{
    i->x = (a.x - xllcorner - (cellsize / 2.0)) / cellsize;
    i->y = (a.y - yllcorner - (cellsize / 2.0)) / cellsize;
}

void ASC_Header::img2asc (const Point& i, Point* a) const
{
    a->x = (i.x * cellsize) + xllcorner + (cellsize / 2.0);
    a->y = (i.y * cellsize) + yllcorner + (cellsize / 2.0);
}

Point ASC_Header::asc2img (const Point& a) const
{
    Point i;
    asc2img (a, &i);
    return i;
}

Point ASC_Header::img2asc (const Point& i) const
{
    Point a;
    img2asc (i, &a);
    return a;
}

string read_string (FILE *fp)
{
    string str (30, '\0');
    fscanf(fp, "%29s\n", str.data ());
    str.resize( strlen (str.data ()));
    return str;
}

string to_lower(string s) {
    for(char &c : s)
        c = tolower(c);
    return s;
}

bool compare_string (string a, string b)
{
    // printf ("%s %s\n", a.data(), b.data());
    return to_lower (a) == to_lower (b);
}

void ASC_Header::read_header (FILE* fp)
{
    int c = 0;
    bool set_xllcenter = false;
    bool set_yllcenter = false;
    bool set_xllcorner = false;
    bool set_yllcorner = false;

    for (int i = 0; i < 6; i++) {
        string s = read_string (fp);
        if (compare_string (string ("ncols"), s))
        {
            fscanf (fp, "%d\n", &width);
            c++;
        }
        if (compare_string (string ("nrows"), s))
        {
            fscanf (fp, "%d\n", &height);
            c++;
        }
        if (compare_string (string ("xllcenter"), s))
        {
            set_xllcenter = true;
            fscanf (fp, "%lf\n", &xllcorner);
            c++;
        }
        if (compare_string (string ("yllcenter"), s))
        {
            set_yllcenter = true;
            fscanf (fp, "%lf\n", &yllcorner);
            c++;
        }
        if (compare_string (string ("xllcorner"), s))
        {
            set_xllcorner = true;
            fscanf (fp, "%lf\n", &xllcorner);
            c++;
        }
        if (compare_string (string ("yllcorner"), s))
        {
            set_yllcorner = true;
            fscanf (fp, "%lf\n", &yllcorner);
            c++;
        }
        if (compare_string (string ("cellsize"), s))
        {
            fscanf (fp, "%lf\n", &cellsize);
            c++;
        }
        if (compare_string (string ("nodata_value"), s))
        {
            fscanf (fp, "%lf\n", &nodata_value);
            c++;
        }
    }

    bool mode_set = false;
    if (set_xllcenter && set_yllcenter)
    {
        xllcorner -= cellsize / 2.0;
        yllcorner -= cellsize / 2.0;
        mode_set = true;
    }
    if (set_xllcorner && set_yllcorner)
        mode_set = true;

    if (c != 6 || !mode_set)
        print_fatal (5, "%s\n", "Syntax error.");
}


ASC_Reader::ASC_Reader (FILE* fp, const ASC_Header& h)
{
    header = h;
    data = (double*) malloc (h.width * h.height * sizeof (double));

    int r;
    double value;
    for (size_t len = 0; len < (size_t)(h.width * h.height); len++)
    {
        r = fscanf (fp, "%lf ", &value);
        if (r != 1)
            print_fatal (1, "Error while reading ASC data: fscanf returned %d @len=%zu\n", r, len);
        data[len] = value;
    }
    r = fscanf (fp, "%lf ", &value);
    if (r != EOF)
        print_fatal (1, "Overfill at the end of ASC data section: fscanf returned %d @len=%zu\n", r, (size_t)(h.width * h.height));
}

ASC_Reader::~ASC_Reader ()
{
    free (data);
}

double ASC_Reader::get_pixel (unsigned x, unsigned y) const
{
    if (x >= (unsigned) header.width || y >= (unsigned) header.height)
        print_fatal (-1,"out of bounds access: (%d,%d). w: %d, h: %d.\n",
                     x, y, header.width, header.height);

    y = header.height - 1 - y;

    double pixel = data[(y * header.width) + x];

    if (pixel == header.nodata_value)
    {
        print_warning ("Found ASC no_data pixel @ %d,%d\n", x, y);
        return lowest<double> ();
    }

    return pixel;
}




// empty dem, no allocation
Dem::Dem () :
    Grid<double> (),
    header (),
    min (0.0),
    max (0.0) {}

// copy dem
Dem::Dem (Dem& dem) :
    Grid<double> (dem.width, dem.height),
    header (dem.header),
    min (dem.min),
    max (dem.max)
{
    for (int i = 0; i < dem.width; i++)
        for (int j = 0; j < dem.height; j++)
            (*this)(i, j, BOUND) = dem (i, j);
}

// empty dem, allocate w * h
Dem::Dem (const int w, const int h) :
    Grid<double> (w, h),
    header (),
    min (0.0),
    max (0.0) {}

// empty dem, allocate from header
Dem::Dem (const ASC_Header header) :
    Grid<double> (header.width, header.height, header.nodata_value),
    header (header),
    min (0.0),
    max (0.0) {}


Dem::Dem (const char* filename) :
    Grid<double> (),
    header (),
    min (0.0),
    max (0.0)
{
    FILE* fp;
    fp = fopen (filename, "r");
    if (fp == NULL)
        print_fatal (2, "Could not open file `%s'. %s\n", filename, strerror (errno));

    header = ASC_Header (fp);
    resize (header.width, header.height);

    ASC_Reader reader (fp, header);

    for (int i = 0; i < header.width; i++)
        for (int j = 0; j < header.height; j++)
            (*this)(i, j, BOUND) = reader.get_pixel (i, j);

    fclose (fp);
    set_minmax ();
}

bool Dem::is_equal (const Coord& a, const Coord& b)
{
    double av = (*this)(a);
    double bv = (*this)(b);

    if (av > bv)
        return false;
    if (av < bv)
        return false;

    return true;
}

double& Dem::operator() (const Coord& c)
{
    return Grid<double>::operator()(c, ABYSS);
}
double& Dem::operator() (const Coord& c, const AccessType a)
{
    return Grid<double>::operator()(c, a);
}
double& Dem::operator() (const int x, const int y, const AccessType a)
{
    return Grid<double>::operator()(x, y, a);
}


void Dem::write (const char* filename, bool round2int)
{
    FILE* fp;
    fp = fopen (filename, "w");
    if (fp == NULL)
        print_fatal (2, "Could not open file `%s'. %s\n", filename, strerror (errno));

    fprintf (fp, "ncols %d\n", header.width);
    fprintf (fp, "nrows %d\n", header.height);
    fprintf (fp, "xllcorner %.17g\n", header.xllcorner);
    fprintf (fp, "yllcorner %.17g\n", header.yllcorner);
    fprintf (fp, "cellsize %.17g\n", header.cellsize);
    fprintf (fp, "NODATA_value %.17g\n", header.nodata_value);

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            int yy = height - 1 - y;
            int xx = x;
            double v = (*this)(xx, yy);

            bool eol = y == height - 1 && x == width -1;
            if (round2int)
                fprintf (fp, eol? "%ld\n" : "%ld ", lround (v));
            else
                fprintf (fp, eol? "%.17g\n" : "%.17g ", v);
        }

    fclose (fp);
}

double quantize_value (double v, double q) {
    if (q <= 0)
        return v;

    double steps = v / q;
    return std::round (steps) * q;
}

void Dem::quantize (double quant)
{
    for (int i = 0; i < width; i++)
        for (int j = 0; j < height; j++)
            (*this) (i, j) = quantize_value ((*this) (i, j), quant);
    set_minmax ();
}

void Dem::scale (double scalefactor)
{
    if (scalefactor == 1.0)
        return;
    for (int i = 0; i < width; i++)
        for (int j = 0; j < height; j++)
            (*this) (i, j) = (*this)(i, j) * scalefactor;
    set_minmax ();
}


void Dem::set_minmax ()
{
    max = lowest<double> ();
    min = highest<double> ();

    for (int i = 0; i < width; i++)
        for (int j = 0; j < height; j++)
        {
            double v = (*this) (i, j);
            if (v < min)
                min = v;
            if (v > max)
                max = v;
        }
}


void Dem::print_info (const char *filename) const
{
    // print file only if not null
    const char* empty = "";
    const char* prev = " File ";
    const char* succ = ". ";
    const char* p = filename? prev : empty;
    const char* s = filename? succ : empty;
    const char* f = filename? filename : empty;

    printf ("ASC info.%s%s%s\n"
            "Header: width %d, height %d, xllcorner: %lf, yllcorner: %lf, "
            "cellsize: %lf, nodata_value: %lf.\n",
            p, f, s,
            header.width, header.height,
            header.xllcorner, header.yllcorner,
            header.cellsize, header.nodata_value);

    Point se = header.img2asc (Point (0, 0));
    Point nw = header.img2asc (Point (header.width-1, header.height-1));
    Point rel = (nw - se) + header.cellsize;
    printf ("Data range: zmin %lf, zmax %lf; Extent (with cellsize): (%lf %lf) -- "
            "[south-east to north-west corners: (%lf %lf) (%lf %lf)]\n",
            min, max, rel.x, rel.y, se.x, se.y, nw.x, nw.y);


    return;
}
