#ifndef FRINGEFIX_PATTERN_H
#define FRINGEFIX_PATTERN_H

#include <windows.h>

/* тестовый паттерн на весь выбранный монитор: слева вертикальные линии, справа горизонтальные,
 * по центру текст; закрывается по Esc или клику. повторный вызов выводит окно вперёд */
void pattern_show(HINSTANCE inst, int monitor);

#endif
