/*************************************************************************/ /*!
@File			innodpu_dp_vcodiv.h
@Title
@Copyright		Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description
@License		Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/


#ifndef _INNO_DP_VCO_H_
#define _INNO_DP_VCO_H_

static unsigned int pixvco_freq[][2] = {
	{42, 1008000},
	{43, 1032000},
	{44, 1056000},
	{45, 1080000},
	{46, 1104000},
	{47, 1128000},
	{48, 1152000},
	{49, 1176000},
	{50, 1200000},
	{51, 1224000},
	{52, 1248000},
	{53, 1272000},
	{54, 1296000},
	{55, 1320000},
	{56, 1344000},
	{57, 1368000},
	{58, 1392000},
	{59, 1416000},
	{60, 1440000},
	{61, 1464000},
	{62, 1488000},
	{63, 1512000},
	{64, 1536000},
	{65, 1560000},
	{66, 1584000},
	{67, 1608000},
	{68, 1632000},
	{69, 1656000},
	{70, 1680000},
	{71, 1704000},
	{72, 1728000},
	{73, 1752000},
	{74, 1776000},
	{75, 1800000},
	{76, 1824000},
	{77, 1848000},
	{78, 1872000},
	{79, 1896000},
	{80, 1920000},
	{81, 1944000},
	{82, 1968000},
	{83, 1992000},
	{84, 2016000},
	{85, 2040000},
	{86, 2064000},
	{87, 2088000},
	{88, 2112000},
	{89, 2136000},
	{90, 2160000},
	{91, 2184000},
	{92, 2208000},
	{93, 2232000},
	{94, 2256000},
	{95, 2280000},
	{96, 2304000},
	{97, 2328000},
	{98, 2352000},
	{99, 2376000},
	{100, 2400000},
	{101, 2424000},
	{102, 2448000},
	{103, 2472000},
	{104, 2496000},
	{105, 2520000},
	{106, 2544000},
	{107, 2568000},
	{108, 2592000},
	{109, 2616000},
	{110, 2640000},
	{111, 2664000},
	{112, 2688000},
	{113, 2712000},
	{114, 2736000},
	{115, 2760000},
	{116, 2784000},
	{117, 2808000},
	{118, 2832000},
	{119, 2856000},
	{120, 2880000},
	{121, 2904000},
	{122, 2928000},
	{123, 2952000},
	{124, 2976000},
	{125, 3000000},
};


static unsigned int pclk_divabc[][4] = {
	/*
	 pclk_2abc,
	 |  pclkdiva,
	 |  |  pclkdivb,
	 |  |  |  pclkdivc,
	 |  |  |  |  */
	{2, 1, 0, 1},
	{4, 1, 1, 1},
	{6, 1, 0, 3},
	{8, 1, 1, 2},
	{10, 1, 0, 5},
	{12, 1, 1, 3},
	{14, 1, 0, 7},
	{16, 1, 1, 4},
	{18, 1, 2, 3},
	{20, 1, 1, 5},
	{22, 1, 0, 11},
	{24, 1, 2, 4},
	{26, 1, 0, 13},
	{28, 1, 1, 7},
	{30, 1, 2, 5},
	{32, 1, 1, 8},
	{34, 1, 0, 17},
	{36, 1, 2, 6},
	{38, 1, 0, 19},
	{40, 1, 3, 4},
	{42, 1, 2, 7},
	{44, 1, 1, 11},
	{46, 1, 0, 23},
	{48, 1, 2, 8},
	{50, 1, 3, 5},
	{52, 1, 1, 13},
	{54, 1, 2, 9},
	{56, 4, 0, 7},
	{58, 1, 0, 29},
	{60, 1, 3, 6},
	{62, 1, 0, 31},
	{64, 1, 1, 16},
	{66, 1, 2, 11},
	{68, 1, 1, 17},
	{70, 1, 3, 7},
	{72, 1, 1, 18},
	{76, 1, 1, 19},
	{78, 1, 2, 13},
	{80, 1, 3, 8},
};


#endif
