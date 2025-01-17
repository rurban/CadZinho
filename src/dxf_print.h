#ifndef _DXF_PRINT_LIB
#define _DXF_PRINT_LIB

#include "pdfgen.h"
#include "miniz.h"
#include "dxf.h"
#include "bmp.h"
#include "graph.h"
#include "list.h"
#include "font.h"
#include "dxf_graph.h"
#include "dxf_image.h"

#define PDF_BUF_SIZE 50*1024*1024

struct txt_buf{
	long pos;
	//char data[PDF_BUF_SIZE];
	char *data;
};

enum print_unit{
	PRT_IN,
	PRT_MM,
	PRT_PX
};

struct page_def{
	enum print_unit unit;
	double w;
	double h;
	char *name;
};

enum print_fmt{
	PRT_PDF,
	PRT_SVG,
	PRT_PNG,
	PRT_JPG,
	PRT_BMP,
	PRT_PS,
	
	PRT_NONE,
	PRT_SIZE
};

struct print_param{
	double w;
	double h;
	double ofs_x;
	double ofs_y;
	double scale;
	double resolution;
	int mono;
	enum print_unit unit;
	bmp_color *list;
	bmp_color *subst;
	int len;
	enum print_fmt out_fmt;
};

void print_img_pdf(struct pdf_doc *pdf, bmp_img *img);

int print_ents_pdf(dxf_drawing *drawing, struct txt_buf *buf , struct print_param param);

int print_pdf(dxf_drawing *drawing, struct print_param param, char *dest);

int print_svg(dxf_drawing *drawing, struct print_param param, char *dest);

int print_img(dxf_drawing *drawing, struct print_param param, char *dest);

int print_ps(dxf_drawing *drawing, struct print_param param, char *dest);

void svg_img_base64(void *context, void *data, int size);

#endif