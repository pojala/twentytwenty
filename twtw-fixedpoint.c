/*
 *  twtw-fixedpoint.c
 *  TwentyTwenty
 *
 *  Created by Pauli Ojala on 5.12.2008.
 *  Copyright 2008 Pauli Olavi Ojala. All rights reserved.
 *
 */
/*
    This file is part of TwentyTwenty.

    TwentyTwenty is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    TwentyTwenty is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TwentyTwenty.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
    Based on work by Tomas Frydrych: http://tthef.net/blog/?p=139
    -- look-up table approach for fixed-point sqrt comes from Allegro library, which is "giftware" licensed.
*/

#include "twtw-fixedpoint.h"


static TwtwFixedNum s_sqrtLUT[] =
{
 0x00000000L, 0x00010000L, 0x00016A0AL, 0x0001BB68L,
 0x00020000L, 0x00023C6FL, 0x00027312L, 0x0002A550L,
 0x0002D414L, 0x00030000L, 0x0003298BL, 0x0003510EL,
 0x000376CFL, 0x00039B05L, 0x0003BDDDL, 0x0003DF7CL,
 0x00040000L, 0x00041F84L, 0x00043E1EL, 0x00045BE1L,
 0x000478DEL, 0x00049524L, 0x0004B0BFL, 0x0004CBBCL,
 0x0004E624L, 0x00050000L, 0x00051959L, 0x00053237L,
 0x00054AA0L, 0x0005629AL, 0x00057A2BL, 0x00059159L,
 0x0005A828L, 0x0005BE9CL, 0x0005D4B9L, 0x0005EA84L,
 0x00060000L, 0x00061530L, 0x00062A17L, 0x00063EB8L,
 0x00065316L, 0x00066733L, 0x00067B12L, 0x00068EB4L,
 0x0006A21DL, 0x0006B54DL, 0x0006C847L, 0x0006DB0CL,
 0x0006ED9FL, 0x00070000L, 0x00071232L, 0x00072435L,
 0x0007360BL, 0x000747B5L, 0x00075935L, 0x00076A8CL,
 0x00077BBBL, 0x00078CC2L, 0x00079DA3L, 0x0007AE60L,
 0x0007BEF8L, 0x0007CF6DL, 0x0007DFBFL, 0x0007EFF0L,
 0x00080000L, 0x00080FF0L, 0x00081FC1L, 0x00082F73L,
 0x00083F08L, 0x00084E7FL, 0x00085DDAL, 0x00086D18L,
 0x00087C3BL, 0x00088B44L, 0x00089A32L, 0x0008A906L,
 0x0008B7C2L, 0x0008C664L, 0x0008D4EEL, 0x0008E361L,
 0x0008F1BCL, 0x00090000L, 0x00090E2EL, 0x00091C45L,
 0x00092A47L, 0x00093834L, 0x0009460CL, 0x000953CFL,
 0x0009617EL, 0x00096F19L, 0x00097CA1L, 0x00098A16L,
 0x00099777L, 0x0009A4C6L, 0x0009B203L, 0x0009BF2EL,
 0x0009CC47L, 0x0009D94FL, 0x0009E645L, 0x0009F32BL,
 0x000A0000L, 0x000A0CC5L, 0x000A1979L, 0x000A261EL,
 0x000A32B3L, 0x000A3F38L, 0x000A4BAEL, 0x000A5816L,
 0x000A646EL, 0x000A70B8L, 0x000A7CF3L, 0x000A8921L,
 0x000A9540L, 0x000AA151L, 0x000AAD55L, 0x000AB94BL,
 0x000AC534L, 0x000AD110L, 0x000ADCDFL, 0x000AE8A1L,
 0x000AF457L, 0x000B0000L, 0x000B0B9DL, 0x000B172DL,
 0x000B22B2L, 0x000B2E2BL, 0x000B3998L, 0x000B44F9L,
 0x000B504FL, 0x000B5B9AL, 0x000B66D9L, 0x000B720EL,
 0x000B7D37L, 0x000B8856L, 0x000B936AL, 0x000B9E74L,
 0x000BA973L, 0x000BB467L, 0x000BBF52L, 0x000BCA32L,
 0x000BD508L, 0x000BDFD5L, 0x000BEA98L, 0x000BF551L,
 0x000C0000L, 0x000C0AA6L, 0x000C1543L, 0x000C1FD6L,
 0x000C2A60L, 0x000C34E1L, 0x000C3F59L, 0x000C49C8L,
 0x000C542EL, 0x000C5E8CL, 0x000C68E0L, 0x000C732DL,
 0x000C7D70L, 0x000C87ACL, 0x000C91DFL, 0x000C9C0AL,
 0x000CA62CL, 0x000CB047L, 0x000CBA59L, 0x000CC464L,
 0x000CCE66L, 0x000CD861L, 0x000CE254L, 0x000CEC40L,
 0x000CF624L, 0x000D0000L, 0x000D09D5L, 0x000D13A2L,
 0x000D1D69L, 0x000D2727L, 0x000D30DFL, 0x000D3A90L,
 0x000D4439L, 0x000D4DDCL, 0x000D5777L, 0x000D610CL,
 0x000D6A9AL, 0x000D7421L, 0x000D7DA1L, 0x000D871BL,
 0x000D908EL, 0x000D99FAL, 0x000DA360L, 0x000DACBFL,
 0x000DB618L, 0x000DBF6BL, 0x000DC8B7L, 0x000DD1FEL,
 0x000DDB3DL, 0x000DE477L, 0x000DEDABL, 0x000DF6D8L,
 0x000E0000L, 0x000E0922L, 0x000E123DL, 0x000E1B53L,
 0x000E2463L, 0x000E2D6DL, 0x000E3672L, 0x000E3F70L,
 0x000E4869L, 0x000E515DL, 0x000E5A4BL, 0x000E6333L,
 0x000E6C16L, 0x000E74F3L, 0x000E7DCBL, 0x000E869DL,
 0x000E8F6BL, 0x000E9832L, 0x000EA0F5L, 0x000EA9B2L,
 0x000EB26BL, 0x000EBB1EL, 0x000EC3CBL, 0x000ECC74L,
 0x000ED518L, 0x000EDDB7L, 0x000EE650L, 0x000EEEE5L,
 0x000EF775L, 0x000F0000L, 0x000F0886L, 0x000F1107L,
 0x000F1984L, 0x000F21FCL, 0x000F2A6FL, 0x000F32DDL,
 0x000F3B47L, 0x000F43ACL, 0x000F4C0CL, 0x000F5468L,
 0x000F5CBFL, 0x000F6512L, 0x000F6D60L, 0x000F75AAL,
 0x000F7DEFL, 0x000F8630L, 0x000F8E6DL, 0x000F96A5L,
 0x000F9ED9L, 0x000FA709L, 0x000FAF34L, 0x000FB75BL,
 0x000FBF7EL, 0x000FC79DL, 0x000FCFB7L, 0x000FD7CEL,
 0x000FDFE0L, 0x000FE7EEL, 0x000FEFF8L, 0x000FF7FEL,
 0x00100000L,
};


TwtwFixedNum twtw_fixed_sqrt(TwtwFixedNum x)
{
    int t = 0;
    int sh = 0;
    unsigned int mask = 0x40000000;

    const unsigned int fract = x & 0x0000ffff;
    const unsigned int fxd255 = FIXD_FROM_INT(255);

    if (x <= 0)
        return 0;

    if (x > fxd255 || x < FIXD_ONE) {
        // count high zero bits (i.e. find highest bit != 0)
        int bit;
#if defined(__armv5te__)
        __asm__("clz  %0, %1 \n"
                "rsb  %0, %0, #31 \n"
                    : "=r"(bit)
                    : "r"(x)
                );

        // make even
        bit &= 0xfffffffe;
#else
        bit = 30;
        while (bit >= 0) {
            if (x & mask)
                break;

            mask = (mask >> 1 | mask >> 2);
            bit -= 2;
        }
#endif

        // shift to maximize table precision
        sh = ((bit - 22) >> 1);
        
        t = (bit >= 8) ? (x >> (16 - 22 + bit))
                       : (x << (22 - 16 - bit));
    }
    else {
        t = FIXD_TO_INT(x);
    }

    // average of two nearest values in LUT
    TwtwFixedNum v1, v2;
    v1 = s_sqrtLUT[t];
    v2 = s_sqrtLUT[t + 1];

    unsigned int d1, d2;
    d1 = fract >> 12;
    d2 = ((unsigned int)FIXD_ONE >> 12) - d1;

    x = ((v1*d2) + (v2*d1)) / (FIXD_ONE >> 12);

    if (sh > 0)
        x = x << sh;
    else if (sh < 0)
        x = (x >> (1 + (~sh)));

    return x;
}

