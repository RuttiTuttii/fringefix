#ifndef FRINGEFIX_GAMEDETECT_H
#define FRINGEFIX_GAMEDETECT_H

#include <windows.h>

enum { GAME_NONE = 0, GAME_FULLSCREEN, GAME_3D };

/* нужна ли оверлею пауза: активное окно закрывает весь монитор без рамки с заголовком (игра в окне без рамки,
 * монопольный полный экран, видео на F11) или, при mode 2, это окно 3D-приложения с загруженным
 * d3d9, d3d11, d3d12, vulkan или opengl вне списка известных не игр. mode 0 выключает проверку.
 * окна самого fringefix, рабочий стол и панель задач не считаются.
 * 3D-приложение с защитой от чтения процесса (античит) определяется только по полному экрану */
int game_detect(int mode, const RECT *monitor);

#endif
