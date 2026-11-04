#ifndef FRINGEFIX_OUTPUT_H
#define FRINGEFIX_OUTPUT_H

#include <windows.h>
#include <dxgi1_6.h>

#define OUTPUT_NAME_LEN 64

/* выходы всех видеокарт нумеруются подряд с нуля, как monitor= в настройках.
 * при успехе отдаёт адаптер и выход с увеличенным счётчиком ссылок; 0 или -1 */
int ff_output_pick(int index, IDXGIAdapter1 **adapter, IDXGIOutput6 **output);

/* заполняет names строками вида "0: \\.\DISPLAY1 (3840x2160)", возвращает их число */
int ff_output_list(wchar_t names[][OUTPUT_NAME_LEN], int max);

/* что известно о мониторе и видеокарте для отчёта; серийных номеров и EDID среди этого нет */
typedef struct {
    unsigned width;
    unsigned height;
    int hdr;
    char gpu[64];
} ff_output_info;

int ff_output_info_get(int index, ff_output_info *info);

/* прямоугольник монитора в координатах рабочего стола; 0 или -1 */
int ff_output_rect(int index, RECT *rect);

#endif
