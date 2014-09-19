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

static float normalize(unsigned h, unsigned *data, int data_cnt) {
	int i;

	max = 0;
	for (i = 0; i < data_cnt; i++) {
		if (data[i] > max)
			max = data[i];
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


void svg(const char *label, int w, int h, int xmargin, int ymargin, unsigned *data, unsigned data_cnt) {
	int i;	
	float scale = normalize(h - 2 * ymargin, data, data_cnt);
	if (scale == 0)
		scale = 1;
	printf("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\"");
	printf(" width=\"%dpx\" height=\"%dpx\">\n", w + 100, h);	
	
	printf("<text style=\"font-size:12px;font-style:normal;font-weight:normal;line-height:125%%;letter-spacing:0px;word-spacing:0px;fill:#000000;fill-opacity:1;stroke:none;font-family:Sans\" x=\"%d\" y=\"%d\">%s</text>\n",
		xmargin, ymargin - 5, label);

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

	int blank = rcpGetStatsInterval();
	float deltax = (float) (w - (2 * xmargin)) / (float) (data_cnt - 1);
	for (i = 0; i < data_cnt - 1; i++) {
		if (i == blank)
			continue;
			
		float stroke = 1.5;
		if (i > blank)
			stroke = 0.5;
		
		printf("<polyline points=\"%.02f, %.02f %.02f,%.02f \" style=\"fill:none;stroke:red;stroke-width:%.02f\" />\n",
			xmargin + i * deltax, h - ymargin - data[i] / scale,
			xmargin + i * deltax + deltax, h - ymargin - data[i + 1] / scale,
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

void svg_hbar(int w, int h) {
	printf("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\"");
	printf(" width=\"%dpx\" height=\"%dpx\">", w, h);

	printf("<rect width=\"%dpx\" height=\"%dpx\" ", w, h);
	printf("style=\"fill:#00ff00;stroke-width:1;stroke:white\" />");
	
	printf("</svg>");
}

void svg_checkmark(int size) {
	float scale = (float) size / (float) 100;

	printf("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\"");
	printf(" width=\"%dpx\" height=\"%dpx\">", size, size);

	printf("<path d=\"M10 10 L10 90 L 90 90 L90 10 L10 10 Z\" ");
	printf("style=\"fill:white;stroke-width:1;stroke:black\" transform=\"scale(%f)\" />", scale);

	printf("<path d=\"M0 50 L50 100 L100 0 L75 3 L45 80 L0 48 L0 50 Z\" ");
	printf("style=\"fill:#00ff00;stroke-width:2;stroke:black\" transform=\"scale(%f)\" />", scale);

	printf("</svg>");

}

void svg_cross(int size) {
	float scale = (float) size / (float) 100;
 
	printf("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\"");
	printf(" width=\"%dpx\" height=\"%dpx\">", size, size);

	printf("<path d=\"M10 10 L10 90 L 90 90 L90 10 L10 10 Z\" ");
	printf("style=\"fill:white;stroke-width:1;stroke:black\" transform=\"scale(%f)\" />", scale);

	printf("<path d=\"M20 0 L0 10 L95 100 L100 95 L20 0 Z\" ");
	printf("style=\"fill:#ff0000;stroke-width:2;stroke:black\" transform=\"scale(%f)\" />", scale);
	printf("<path d=\"M0 95 L5 100 L100 15 L90 0 L0 95 Z\" ");
	printf("style=\"fill:#ff0000;stroke-width:2;stroke:black\" transform=\"scale(%f)\" />", scale);

	printf("</svg>");

}
