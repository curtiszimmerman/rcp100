/*
 * Copyright (C) 2012-2013 RCP100 Team (rcpteam@yahoo.com)
 *
 * This file is part of RCP100 project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "http.h"

static unsigned max = 0;

static float normalize(unsigned h, unsigned *data1, unsigned *data2, int data_cnt) {
	int i;

	max = 0;
	for (i = 0; i < data_cnt; i++) {
		if (data1[i] > max)
			max = data1[i];
		if (data2[i] > max)
			max = data2[i];
	}

	if (max > 80 && max <= 100)
		return (float) 100 / (float) h;

	// adjust max
	unsigned tmp = 10; 
	while (1) {
		tmp *= 2;
		if (max <= tmp) {
			max = tmp;
			return (float) tmp / (float) h;
		}
	}
	return 0;
}


void svg2(const char *label1, const char *label2, int w, int h, int xmargin, int ymargin, unsigned *data1, unsigned *data2, unsigned data_cnt) {
	int i;	
	float scale = normalize(h - 2 * ymargin, data1, data2, data_cnt);
	if (scale == 0)
		scale = 1;
	printf("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\"");
	printf(" width=\"%dpx\" height=\"%dpx\">\n", w + 100, h);	
	
		
	printf("<text style=\"font-size:12px;font-style:normal;font-weight:lighter;line-height:125%%;letter-spacing:0px;word-spacing:0px;fill:#000000;fill-opacity:1;stroke:red;font-family:Sans\" x=\"%d\" y=\"%d\">%s</text>\n",
		xmargin, ymargin - 5, label1);
	printf("<text style=\"font-size:12px;font-style:normal;font-weight:lighter;line-height:125%%;letter-spacing:0px;word-spacing:0px;fill:#000000;fill-opacity:1;stroke:blue;font-family:Sans\" x=\"%d\" y=\"%d\">%s</text>\n",
		xmargin, ymargin + 15, label2);

	// print grid
	printf("<polyline points=\"%d, %d %d,%d %d,%d %d,%d %d,%d\" style=\"fill:none;stroke:grey;stroke-width:2\" />\n",
		xmargin, ymargin, w - xmargin, ymargin,
		w - xmargin, h - ymargin, xmargin, h - ymargin,
		xmargin, ymargin);

	for (i = 1; i < 4; i++) {
		unsigned delta = ((h - (2 * ymargin)) / 4) * i;
		printf("<polyline points=\"%d, %d %d,%d \" style=\"fill:none;stroke:grey;stroke-width:0.5\" />\n",
			xmargin, ymargin + delta, w - xmargin, ymargin + delta);
	}
	
	unsigned delta = (w - (2 * xmargin)) / 8;
	printf("<text style=\"font-size:12px;font-style:normal;font-weight:normal;line-height:125%%;letter-spacing:0px;word-spacing:0px;fill:#000000;fill-opacity:1;stroke:none;font-family:Sans\" x=\"%d\" y=\"%d\">%d</text>\n",
		xmargin - 5, h - ymargin + 15, 0);
	for (i = 1; i < 8; i++) {
		printf("<polyline points=\"%d, %d %d,%d \" style=\"fill:none;stroke:grey;stroke-width:0.5\" />\n",
			xmargin + delta * i, ymargin, xmargin + delta * i, h - ymargin);
		
		printf("<text style=\"font-size:12px;font-style:normal;font-weight:normal;line-height:125%%;letter-spacing:0px;word-spacing:0px;fill:#000000;fill-opacity:1;stroke:none;font-family:Sans\" x=\"%d\" y=\"%d\">%d</text>\n",
			xmargin + (delta * i) - 5, h - ymargin + 15, i * 3);
	}
	printf("<text style=\"font-size:12px;font-style:normal;font-weight:normal;line-height:125%%;letter-spacing:0px;word-spacing:0px;fill:#000000;fill-opacity:1;stroke:none;font-family:Sans\" x=\"%d\" y=\"%d\">%d</text>\n",
		xmargin + (delta * i) - 5, h - ymargin + 15, i * 3);

	// print graph
	int blank = rcpGetStatsInterval();
	float deltax = (float) (w - (2 * xmargin)) / (float) (data_cnt - 1);
	for (i = 0; i < data_cnt - 1; i++) {
		if (i == blank)
			continue;
			
		float stroke = 1.5;
		if (i > blank)
			stroke = 0.5;
		
		printf("<polyline points=\"%.02f, %.02f %.02f,%.02f \" style=\"fill:none;stroke:red;stroke-width:%.02f\" />\n",
			xmargin + i * deltax, h - ymargin - data1[i] / scale,
			xmargin + i * deltax + deltax, h - ymargin - data1[i + 1] / scale,
			stroke);

	}
	for (i = 0; i < data_cnt - 1; i++) {
		if (i == blank)
			continue;
			
		float stroke = 1.5;
		if (i > blank)
			stroke = 0.5;
		
		printf("<polyline points=\"%.02f, %.02f %.02f,%.02f \" style=\"fill:none;stroke:blue;stroke-width:%.02f\" />\n",
			xmargin + i * deltax, 1 + h - ymargin - data2[i] / scale, // y shifted by 1
			xmargin + i * deltax + deltax, 1 + h - ymargin - data2[i + 1] / scale, // y shifted by 1
			stroke);

	}
	max /= 4;
	unsigned yoffset = (h - 2 * ymargin) / 4;
	for (i = 1; i < 5; i++) {
		printf("<text style=\"font-size:12px;font-style:normal;font-weight:normal;line-height:125%%;letter-spacing:0px;word-spacing:0px;fill:#000000;fill-opacity:1;stroke:none;font-family:Sans\" x=\"%d\" y=\"%d\">%d</text>\n",
			w - xmargin + 3, h - ymargin + 5 - yoffset * i,  i * max);
	}
	
	printf("</svg>\n");
}
