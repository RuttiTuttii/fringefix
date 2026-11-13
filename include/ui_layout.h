#ifndef FRINGEFIX_UI_LAYOUT_H
#define FRINGEFIX_UI_LAYOUT_H

#include "i18n.h"

/* геометрия окна настроек при 96 dpi. карточки рисуются по этим таблицам, элементы встают по ним же,
 * при другом dpi всё умножается на масштаб */
enum {
    MARGIN = 16,
    CARD_W = 488,
    CARD_HEAD = 26,
    ROW_H = 30,
    CARD_PAD = 10,
    CARD_GAP = 8,
    /* подсказка над карточками */
    TOP = 34,
    /* колонки внутри карточки */
    COL_LABEL = 14,
    COL_SLIDER = 150,
    SLIDER_W = 236,
    COL_EDIT = 396,
    EDIT_W = 58,
    COL_UNIT = 460,
    CTRL_H = 28
};

typedef struct {
    str_id title;
    int rows;
} card_def;

/* idx 0..5 это shift[idx / 2][idx % 2], дальше резкость и список мониторов */
#define IDX_SHARPEN 6
#define IDX_MONITOR 7

typedef struct {
    str_id label;
    int idx;
    int card;
    int row;
} row_def;

/* три цвета, под ними карточка изображения: выбор монитора и резкость; всё выше относится к выбранному монитору */
static const card_def CARDS[] = {
    { S_RED, 2 }, { S_GREEN, 2 }, { S_BLUE, 2 }, { S_IMAGE, 2 },
};
#define NCARDS ((int)(sizeof CARDS / sizeof CARDS[0]))

static const row_def ROWS[] = {
    { S_HORIZONTAL, 0, 0, 0 }, { S_VERTICAL, 1, 0, 1 },
    { S_HORIZONTAL, 2, 1, 0 }, { S_VERTICAL, 3, 1, 1 },
    { S_HORIZONTAL, 4, 2, 0 }, { S_VERTICAL, 5, 2, 1 },
    { S_MONITOR, IDX_MONITOR, 3, 0 }, { S_SHARPEN, IDX_SHARPEN, 3, 1 },
};
#define NROWS ((int)(sizeof ROWS / sizeof ROWS[0]))

static inline int card_h(int c)
{
    return CARD_HEAD + CARDS[c].rows * ROW_H + CARD_PAD;
}

static inline int card_y(int c)
{
    int y = TOP;
    int i;

    for (i = 0; i < c; i++) {
        y += card_h(i) + CARD_GAP;
    }
    return y;
}

static inline int row_y(const row_def *r)
{
    return card_y(r->card) + CARD_HEAD + r->row * ROW_H;
}

/* всё, что ниже карточек: строка состояния с кнопкой «О программе», две строки кнопок и выключатель фильтра,
 * который стоит в самом низу под левой кнопкой */
#define CARDS_BOTTOM (card_y(NCARDS - 1) + card_h(NCARDS - 1))
#define STATUS_Y (CARDS_BOTTOM + 12)
#define BTN1_Y (CARDS_BOTTOM + 40)
#define BTN2_Y (BTN1_Y + 34)
#define BTN_H 30
#define POWER_Y (BTN2_Y + BTN_H + 14)
#define CLIENT_W (MARGIN * 2 + CARD_W)
#define CLIENT_H (POWER_Y + CTRL_H + 16)

/* круглая кнопка «?» справа в строке состояния */
#define ABOUT_D 22

#endif
