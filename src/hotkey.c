#include "hotkey.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VK_F1_CODE 0x70
#define FKEY_MAX 24
#define TOKEN_MAX 16

static const struct {
    const char *name;
    unsigned bit;
} MODS[] = {
    { "Ctrl", FF_MOD_CTRL }, { "Alt", FF_MOD_ALT }, { "Shift", FF_MOD_SHIFT }, { "Win", FF_MOD_WIN },
};
#define NMODS ((int)(sizeof MODS / sizeof MODS[0]))

static int equals_ci(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
    }
    return *a == *b;
}

/* модификатор по имени; "control" принимается как синоним ctrl */
static unsigned mod_bit(const char *t)
{
    int i;

    if (equals_ci(t, "control")) {
        return FF_MOD_CTRL;
    }
    for (i = 0; i < NMODS; i++) {
        if (equals_ci(t, MODS[i].name)) {
            return MODS[i].bit;
        }
    }
    return 0;
}

/* код клавиши по имени: A-Z, 0-9 и F1-F24; 0 если такой нет */
static unsigned key_code(const char *t)
{
    if (strlen(t) == 1 && isalnum((unsigned char)t[0])) {
        return (unsigned)toupper((unsigned char)t[0]);
    }
    if ((t[0] == 'f' || t[0] == 'F') && t[1] >= '1' && t[1] <= '9') {
        char *end;
        long n = strtol(t + 1, &end, 10);

        if (*end == '\0' && n >= 1 && n <= FKEY_MAX) {
            return (unsigned)(VK_F1_CODE + n - 1);
        }
    }
    return 0;
}

/* следующий токен между плюсами без пробелов по краям; NULL, когда строка кончилась */
static const char *next_token(const char *s, char *tok, int *last)
{
    size_t n = 0;

    while (*s == ' ' || *s == '\t') {
        s++;
    }
    while (*s && *s != '+') {
        if (n + 1 >= TOKEN_MAX) {
            return NULL;
        }
        tok[n++] = *s++;
    }
    while (n > 0 && (tok[n - 1] == ' ' || tok[n - 1] == '\t')) {
        n--;
    }
    tok[n] = '\0';
    *last = *s == '\0';
    return *s ? s + 1 : s;
}

int ff_hotkey_parse(const char *s, ff_hotkey *out)
{
    ff_hotkey h = { 0, 0 };
    char tok[TOKEN_MAX];
    int last = 0;

    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '\0' || equals_ci(s, "none")) {
        *out = h;
        return 0;
    }
    while (!last) {
        unsigned bit;

        s = next_token(s, tok, &last);
        if (!s || tok[0] == '\0') {
            return -1;
        }
        bit = mod_bit(tok);
        if (bit && !last) {
            h.mods |= bit;
        } else if (!bit && last) {
            h.vk = key_code(tok);
        } else {
            return -1;
        }
    }
    /* клавиши F1-F24 можно без модификатора, остальным он обязателен */
    if (h.vk == 0 || (h.mods == 0 && h.vk < VK_F1_CODE)) {
        return -1;
    }
    *out = h;
    return 0;
}

int ff_hotkey_format(const ff_hotkey *h, char *buf, size_t n)
{
    size_t used = 0;
    int w;
    int i;

    if (h->vk == 0) {
        w = snprintf(buf, n, "none");
        return (w < 0 || (size_t)w >= n) ? -1 : w;
    }
    for (i = 0; i < NMODS; i++) {
        if (h->mods & MODS[i].bit) {
            w = snprintf(buf + used, n - used, "%s+", MODS[i].name);
            if (w < 0 || used + (size_t)w >= n) {
                return -1;
            }
            used += (size_t)w;
        }
    }
    if (h->vk >= VK_F1_CODE && h->vk < VK_F1_CODE + FKEY_MAX) {
        w = snprintf(buf + used, n - used, "F%u", h->vk - VK_F1_CODE + 1);
    } else {
        w = snprintf(buf + used, n - used, "%c", (char)h->vk);
    }
    if (w < 0 || used + (size_t)w >= n) {
        return -1;
    }
    return (int)(used + (size_t)w);
}
