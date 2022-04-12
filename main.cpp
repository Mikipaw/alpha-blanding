#include "TXLib.h"
#include <emmintrin.h>
#include <smmintrin.h>

//-------------------------------------------------------------------------------------------------

typedef RGBQUAD (&scr_t) [600][800];

inline scr_t LoadImage (const char* filename)
{
    RGBQUAD* mem = nullptr;
    HDC dc = txCreateDIBSection (800, 600, &mem);
    txBitBlt (dc, 0, 0, 0, 0, dc, 0, 0, BLACKNESS);

    HDC image = txLoadImage (filename);
    txBitBlt (dc, (txGetExtentX (dc) - txGetExtentX (image)) / 2,
              (txGetExtentY (dc) - txGetExtentY (image)) / 2, 0, 0, image);
    txDeleteDC (image);

    return (scr_t) *mem;
}

//-------------------------------------------------------------------------------------------------

const unsigned char
        Z = 0x80u;
const char I = (255u);

const __m128i ZERO =                  _mm_set_epi8 (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

//Sign extend packed 8-bit integers in a to packed 16-bit integers, and store the results in dst.
const __m128i FULL = _mm_cvtepu8_epi16 (_mm_set_epi8 (I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I));


//-------------------------------------------------------------------------------------------------

int main()
{
    system("mode con cols=50 lines=25");
    txCreateWindow (800, 600);
    Win32::_fpreset();
    txBegin();

    auto front = (scr_t) LoadImage ("withgs.bmp");
    auto back  = (scr_t) LoadImage ("table.bmp");
    auto scr   = (scr_t) *txVideoMemory();

    for (int n = 0; ; n++)
    {
        if (GetAsyncKeyState (VK_ESCAPE)) break;

        if (!GetKeyState (VK_CAPITAL))
        {
            for (int y = 0; y < 600; y++)
                for (int x = 0; x < 800; x += 4)
                {
                    //-----------------------------------------------------------------------
                    //       15 14 1312   11 10  9  8    7  6  5  4    3  2  1  0
                    // fr = [r3 g3 b3 a3 | r2 g2 b2 a2 | r1 g1 b1 a1 | r0 g0 b0 a0]
                    //-----------------------------------------------------------------------

                    //Load 128-bits of integer data from unaligned memory into dst.
                    __m128i fr = _mm_lddqu_si128 ((__m128i*) &front[y][x]);                   // fr = front[y][x]
                    __m128i bk = _mm_lddqu_si128 ((__m128i*) &back [y][x]);

                    //-----------------------------------------------------------------------
                    //       15 14 13 12   11 10  9  8    7  6  5  4    3  2  1  0
                    // fr = [a3 r3 g3 b3 | a2 r2 g2 b2 | a1 r1 g1 b1 | a0 r0 g0 b0]
                    //        \  \  \  \    \  \  \  \   xx xx xx xx   xx xx xx xx
                    //         \  \  \  \    \  \  \  \.
                    //          \  \  \  \    '--+--+--+-------------+--+--+--.
                    //           '--+--+--+------------+--+--+--.     \  \  \  \.
                    //                                  \  \  \  \     \  \  \  \.
                    // FR = [-- -- -- -- | -- -- -- -- | a3 r3 g3 b3 | a2 r2 g2 b2]
                    //-----------------------------------------------------------------------

                    /*
                     * Move the upper 2 single-precision (32-bit) floating-point elements from fr
                     * to the lower 2 elements of FR,
                     * and copy the upper 2 elements from ZERO to the upper 2 elements of FR.
                     */

                    auto FR = (__m128i) _mm_movehl_ps ((__m128) ZERO, (__m128) fr);       // FR = (fr >> 8*8)
                    auto BK = (__m128i) _mm_movehl_ps ((__m128) ZERO, (__m128) bk);

                    //-----------------------------------------------------------------------
                    //       15 14 13 12   11 10  9  8    7  6  5  4    3  2  1  0
                    // fr = [a3 r3 g3 b3 | a2 r2 g2 b2 | a1 r1 g1 b1 | a0 r0 g0 b0]
                    //       xx xx xx xx   xx xx xx xx                 /  /   |  |
                    //                                         _______/  /   /   |
                    //            ...   ...     ...           /     ____/   /    |
                    //           /     /       /             /     /       /     |
                    // fr = [-- a1 -- r1 | -- g1 -- b1 | -- a0 -- r0 | -- g0 -- b0]
                    //-----------------------------------------------------------------------


                    //Zero extend packed unsigned 8-bit integers in fr to packed 16-bit integers, and store the results in fr.
                    fr = _mm_cvtepu8_epi16 (fr);                                               // fr[i] = (WORD) fr[i]
                    FR = _mm_cvtepu8_epi16 (FR);

                    bk = _mm_cvtepu8_epi16 (bk);
                    BK = _mm_cvtepu8_epi16 (BK);

                    //-----------------------------------------------------------------------
                    //       15 14 13 12   11 10  9  8    7  6  5  4    3  2  1  0
                    // fr = [-- a1 -- r1 | -- g1 -- b1 | -- a0 -- r0 | -- g0 -- b0]
                    //          |___________________        |___________________
                    //          |     \      \      \       |     \      \      \.
                    // a  = [-- a1 -- a1 | -- a1 -- a1 | -- a0 -- a0 | -- a0 -- a0]
                    //-----------------------------------------------------------------------

                    static const __m128i moveA = _mm_set_epi8 ((char) Z, 14, (char) Z, 14, (char) Z, 14, (char) Z, 14,
                                                               (char) Z, 6, (char) Z, 6, (char) Z, 6, (char) Z, 6);
                    __m128i a = _mm_shuffle_epi8 (fr, moveA);                                // a [for r0/b0/b0...] = a0...
                    __m128i A = _mm_shuffle_epi8 (FR, moveA);

                    //-----------------------------------------------------------------------

                    /*
                     * Multiply the packed 16-bit integers in fr and a,
                     * producing intermediate 32-bit integers,
                     * and store the low 16 bits of the intermediate integers in fr.
                     */

                    fr = _mm_mullo_epi16 (fr, a);                                           // fr *= a
                    FR = _mm_mullo_epi16 (FR, A);

                    //Subtract packed 16-bit integers in second from packed 16-bit integers in bk, and store the results in bk.
                    bk = _mm_mullo_epi16 (bk, _mm_sub_epi16 (FULL, a));                                  // bk *= (255-a)
                    BK = _mm_mullo_epi16 (BK, _mm_sub_epi16 (FULL, A));


                    //Add packed 16-bit integers in fr and bk, and store the results in sum.
                    __m128i sum = _mm_add_epi16 (fr, bk);                                       // sum = fr*a + bk*(255-a)
                    __m128i SUM = _mm_add_epi16 (FR, BK);

                    //-----------------------------------------------------------------------
                    //        15 14 13 12   11 10  9  8    7  6  5  4    3  2  1  0
                    // sum = [A1 a1 R1 r1 | G1 g1 B1 b1 | A0 a0 R0 r0 | G0 g0 B0 b0]
                    //         \     \       \     \       \_____\_______\_____\.
                    //          \_____\_______\_____\______________    \  \  \  \.
                    //                                    \  \  \  \    \  \  \  \.
                    // sum = [-- -- -- -- | -- -- -- -- | A1 R1 G1 B1 | A0 R0 G0 B0]
                    //-----------------------------------------------------------------------

                    static const __m128i moveSum = _mm_set_epi8 ((char)Z, (char)Z, (char)Z, (char)Z, (char)Z, (char)Z, (char)Z, (char)Z,
                                                                 15, 13, 11, 9, 7, 5, 3, 1);

                    /*
                     * Shuffle packed 8-bit integers in sum
                     * according to shuffle control mask in the corresponding 8-bit element of moveSum,
                     * and store the results in sum.
                     */

                    sum = _mm_shuffle_epi8 (sum, moveSum);                                      // sum[i] = (sium[i] >> 8) = (sum[i] / 256)
                    SUM = _mm_shuffle_epi8 (SUM, moveSum);

                    //-----------------------------------------------------------------------
                    //          15 14 13 12   11 10  9  8    7  6  5  4    3  2  1  0
                    // sum   = [-- -- -- -- | -- -- -- -- | a1 r1 g1 b1 | a0 r0 g0 b0] ->-.
                    // sumHi = [-- -- -- -- | -- -- -- -- | a3 r3 g3 b3 | a2 r2 g2 b2]    |
                    //                                      /  /  /  /    /  /  /  /      V
                    //             .--+--+--+----+--+--+--++--+--+--+----+--+--+--'       |
                    //            /  /  /  /    /  /  /  /    ____________________________/
                    //           /  /  /  /    /  /  /  /    /  /  /  /    /  /  /  /
                    // color = [a3 r3 g3 b3 | a2 r2 g2 b2 | a1 r1 g1 b1 | a0 r0 g0 b0]
                    //-----------------------------------------------------------------------


                    /*
                     * Move the lower 2 single-precision (32-bit) floating-point elements from SUM
                     * to the upper 2 elements of color,
                     * and copy the lower 2 elements from sum to the lower 2 elements of color.
                     */
                    auto color = (__m128i) _mm_movelh_ps ((__m128) sum, (__m128) SUM);  // color = (sumHi << 8*8) | sum

                    /*
                     * Store 128-bits of integer data from color into memory. scr
                     * does not need to be aligned on any particular boundary.
                     */
                    _mm_storeu_si128 ((__m128i*) &scr[y][x], color);
                }
        }
        else
        {
            for (int y = 0; y < 600; y++)
                for (int x = 0; x < 800; x++)
                {
                    RGBQUAD* bk = &back [y][x];
                    RGBQUAD* fr = &front[y][x];

                    uint16_t a  = fr->rgbReserved;

                    scr[y][x]   = { (BYTE) ( (fr->rgbBlue  * (a) + bk->rgbBlue  * (255-a)) >> 8 ),
                                    (BYTE) ( (fr->rgbGreen * (a) + bk->rgbGreen * (255-a)) >> 8 ),
                                    (BYTE) ( (fr->rgbRed   * (a) + bk->rgbRed   * (255-a)) >> 8 ) };
                }
        }

        txUpdateWindow();
        if (!(n % 10)) printf ("%.0lf                         \r", txGetFPS() * 10);
    }

    txDisableAutoPause();
}