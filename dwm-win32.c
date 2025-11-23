/* See LICENSE file for copyright and license details.
 * See HISTORY for an outline of contributions.
 *
 * This is a port of the popular X11 window manager dwm to Microsoft Windows.
 * It was originally started by Marc Andre Tanner <mat at brain-dump dot org>
 *
 * Each child of the root window is called a client. Clients are organized 
 * in a linked client list on each monitor, the focus history is remembered
 * through a stack list on each monitor. Each client contains a bit array
 * to indicate it's tags.
 * 
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading WinMain().
 */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT            0x0600

#if _MSC_VER
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")
#endif

#include <windows.h>
#include <dwmapi.h>
#include <winuser.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <shellapi.h>
#include <stdbool.h>
#include <time.h>

#define NAME                    L"dwm-WIN"     /* Used for window name/class */

#define ISFOCUSABLE(x)          (!(x)->isminimized && ISVISIBLE(x) && IsWindowVisible((x)->hwnd))
#define LENGTH(x)               (sizeof x / sizeof x[0])
#define MAX(a, b)               ((a) > (b) ? (a) : (b))
#define MIN(a, b)               ((a) < (b) ? (a) : (b))
#define MAXTAGLEN               16
#define WIDTH(x)                ((x)->w + 2 * (x)->bw)
#define HEIGHT(x)               ((x)->h + 2 * (x)->bw)
#define TAGMASK                 ((int)((1LL << LENGTH(tags)) - 1))
#define TEXTW(x)                (textnw(x, wcslen(x)))

#ifdef DEBUG
#define debug(...) eprint(false, __VA_ARGS__)
#else
#define debug(...) do { } while (false)
#endif

#define die(...) if (TRUE) { eprint(true, __VA_ARGS__); eprint(true, L"Win32 Last Error: %d", GetLastError()); cleanup(); exit(EXIT_FAILURE); }

#define EVENT_OBJECT_UNCLOAKED 0x8018

enum { CurNormal, CurResize, CurMove, CurLast };        /* cursor */
enum { ColBorder, ColFG, ColBG, ColLast };              /* color */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle };    /* clicks */

typedef struct Monitor Monitor;

typedef struct {
    int x, y, w, h;
    unsigned long norm[ColLast];
    unsigned long sel[ColLast];
    HDC hdc;
} DC;

DC dc;

typedef union {
    int i;
    unsigned int ui;
    float f;
    void *v;
} Arg;

typedef struct {
    unsigned int click;
    unsigned int button;
    unsigned int key;
    void (*func)(const Arg *arg);
    const Arg arg;
} Button;

typedef struct Client Client;
struct Client {
    HWND hwnd;
    HWND parent;
    HWND root;
    DWORD threadid;
    DWORD processid;
    const wchar_t *processname;
    int x, y, w, h;
    int bw;
    unsigned int tags;
    bool isminimized;
    bool isfloating;
    bool isalive;
    bool ignore;
    bool ignoreborder;
    bool border;
    bool wasvisible;
    bool isfixed, isurgent;
    bool iscloaked;
    Monitor *mon;
    Client *next;
    Client *snext;
};

typedef struct {
    unsigned int mod;
    unsigned int key;
    void (*func)(const Arg *);
    const Arg arg;
} Key;

typedef struct {
    const wchar_t *symbol;
    void (*arrange)(void);
} Layout;

typedef struct {
    const wchar_t *class;
    const wchar_t *title;
    unsigned int tags;
    bool isfloating;
    bool ignoreborder;
} Rule;

struct Monitor {
    HMONITOR hmon;
    MONITORINFOEXW mi;
    int sx, sy, sw, sh; /* screen geometry for this monitor */
    int by, bh, blw;
    int wx, wy, ww, wh; /* window area */
    HWND barhwnd;
    Monitor *next;

    unsigned int tagset[2];
    unsigned int seltags;
    Layout *lt[9]; /* Layouts (and max size) */
    unsigned int sellt;
};

/* function declarations */
static void applyrules(Client *c);
static void arrange(void);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void cleanup();
static void clearurgent(Client *c);
static void detach(Client *c);
static void detachstack(Client *c);
static void drawbar(Monitor *m);
static void drawsquare(bool filled, bool empty, bool invert, unsigned long col[ColLast]);
static void drawtext(const wchar_t *text, unsigned long col[ColLast], bool invert);
void drawborder(Client *c, COLORREF color);
void eprint(bool premortem, const wchar_t *errstr, ...);
static void focus(Client *c);
static void focusstack(const Arg *arg);
static void movestack(const Arg *arg);
static Client *getclient(HWND hwnd);
LPWSTR getclientclassname(HWND hwnd);
LPWSTR getclienttitle(HWND hwnd);
HWND getroot(HWND hwnd);
static void grabkeys(HWND hwnd);
static void killclient(const Arg *arg);
static Client *manage(HWND hwnd);
static void monocle(void);
static Client *nextchild(Client *p, Client *c);
static Client *nexttiled(Client *c);
static void quit(const Arg *arg);
static void resize(Client *c, int x, int y, int w, int h);
static void restack(void);
static BOOL CALLBACK scan(HWND hwnd, LPARAM lParam);
static void setborder(Client *c, bool border);
static void setvisibility(HWND hwnd, bool visibility);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(HINSTANCE hInstance);
static void setbar(HINSTANCE hInstance, Monitor *m);
static void showclientinfo(const Arg *arg); 
static void showhide(Client *c);
static void spawn(const Arg *arg);
static void tag(const Arg *arg);
static int textnw(const wchar_t *text, unsigned int len);
static void tile(void);
static void togglebar(const Arg *arg);
static void toggleborder(const Arg *arg);
static void toggleexplorer(const Arg *arg);
static void togglefloating(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void unmanage(Client *c);
static void updatebars(void);
static void updategeom(void);
static void buildmonitors(void);
static BOOL CALLBACK monenumproc(HMONITOR hMon, HDC hdc, LPRECT lprc, LPARAM lParam);
static Monitor *monitor_from_hwnd(HWND hwnd);
static Monitor *monitor_from_point(POINT pt);
static void view(const Arg *arg);
static void zoom(const Arg *arg);
static bool iscloaked(HWND hwnd);
static void focusmon(const Arg *arg);
static void sendmon(const Arg *arg);

typedef BOOL (*RegisterShellHookWindowProc) (HWND);

static HWND dwmhwnd;
static HWINEVENTHOOK wineventhook;
static HFONT font;

static wchar_t stext[256];

static Monitor *curmon = NULL;

/* Legacy global fallbacks */
static unsigned int seltags = 0, sellt = 0;
static Layout* lt[] = { NULL, NULL };

static Client *clients = NULL;
static Client *sel = NULL;
static Client *stack = NULL;

static UINT shellhookid;

static Monitor *mons = NULL;
static Monitor *selmon = NULL;

/* configuration, allows nested code to access above variables */
#include "config.h"

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { wchar_t limitexceeded[sizeof(unsigned int) * 8 < LENGTH(tags) ? -1 : 1]; };

/* elements of the window whose color should be set to the values in the array below */
static int colorwinelements[] = { COLOR_ACTIVEBORDER, COLOR_INACTIVEBORDER };
static COLORREF colors[2][LENGTH(colorwinelements)] = { 
    { 0, 0 }, /* used to save the values before dwm started */
    { selbordercolor, normbordercolor },
};

static inline bool
isvisible(Client *x) {
    if (!x) return false;
    unsigned int ts;
    if (x->mon)
        ts = x->mon->tagset[x->mon->seltags];
    else
        ts = tagset[seltags];
    return (x->tags & ts) != 0;
}
#define ISVISIBLE(x) (isvisible(x))

/* get the layout pointer for a monitor (fallback to global lt[]) */
static inline Layout *
mon_get_layout(Monitor *m, unsigned int idx) {
    if (m && m->lt[idx]) return m->lt[idx];
    if (idx < 2) return lt[idx];
    return lt[0];
}

void
applyrules(Client *c) {
    unsigned int i;
    Rule *r;

    /* rule matching */
    for (i = 0; i < LENGTH(rules); i++) {
        r = &rules[i];
        if ((!r->title || wcsstr(getclienttitle(c->hwnd), r->title))
        && (!r->class || wcsstr(getclientclassname(c->hwnd), r->class))) {
            c->isfloating = r->isfloating;
            c->ignoreborder = r->ignoreborder;
            unsigned int default_ts = tagset[seltags];
            if (c->mon)
                default_ts = c->mon->tagset[c->mon->seltags];
            c->tags |= r->tags & TAGMASK ? r->tags & TAGMASK : default_ts; 
        }
    }
    if (!c->tags) {
        unsigned int default_ts = tagset[seltags];
        if (c->mon)
            default_ts = c->mon->tagset[c->mon->seltags];
        c->tags = default_ts;
    }
}

void
arrange(void) {
    showhide(stack);
    focus(NULL);
    for (Monitor *m = mons; m; m = m->next) {
        arrangemon(m);
    }
    restack();
}

void
arrangemon(Monitor *m) {
    curmon = m;

    int wx = m->wx, wy = m->wy, ww = m->ww, wh = m->wh, bh = m->bh;

    if (mon_get_layout(m, m->sellt)->arrange)
        mon_get_layout(m, m->sellt)->arrange();

    drawbar(m);
}

void
attach(Client *c) {
    c->next = clients;
    clients = c;
}

void
attachstack(Client *c) {
    c->snext = stack;
    stack = c;
}

static Monitor *
bar_monitor_from_hwnd(HWND bar_hwnd) {
    for (Monitor *m = mons; m; m = m->next) {
        if (m->barhwnd == bar_hwnd) return m;
    }
    return selmon ? selmon : mons;
}

void
buttonpress(unsigned int button, POINTS *point, Monitor *m) {
    unsigned int i, x, click;
    Arg arg = {0};

    /* XXX: hack */
    dc.hdc = GetWindowDC(m->barhwnd);

    i = x = 0;

    do { x += TEXTW(tags[i]); } while (point->x >= x && ++i < LENGTH(tags));
    if (i < LENGTH(tags)) {
        click = ClkTagBar;
        arg.ui = 1 << i;
    }
    else if (point->x < x + m->blw)
        click = ClkLtSymbol;
    else if (point->x > m->wx + m->ww - TEXTW(stext))
        click = ClkStatusText;
    else
        click = ClkWinTitle;

    if (GetKeyState(VK_SHIFT) < 0)
        return;

    for (i = 0; i < LENGTH(buttons); i++) {
        if (click == buttons[i].click && buttons[i].func && buttons[i].button == button
            && (!buttons[i].key || GetKeyState(buttons[i].key) < 0)) {
            selmon = m;
            buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
            break;
        }
    }
}

void
cleanup() {
    int i;

    /* kill timers on bars */
    for (Monitor *m = mons; m; m = m->next) {
        if (m->barhwnd) KillTimer(m->barhwnd, 1);
    }

    for (i = 0; i < LENGTH(keys); i++) {
        UnregisterHotKey(dwmhwnd, i);
    }

    DeregisterShellHookWindow(dwmhwnd);

    if (wineventhook != NULL)
        UnhookWinEvent(wineventhook);

    /* show everything before exit */
    Arg a = {.ui = ~0};
    Layout foo = { L"", NULL };
    view(&a);
    lt[sellt] = &foo;
    while (stack)
        unmanage(stack);

    SetSysColors(LENGTH(colorwinelements), colorwinelements, colors[0]); 

    /* destroy bars */
    for (Monitor *m = mons; m; m = m->next) {
        if (m->barhwnd) DestroyWindow(m->barhwnd);
    }

    DestroyWindow(dwmhwnd);

    HWND hwnd;
    hwnd = FindWindowW(L"Shell_TrayWnd", NULL);
    if (hwnd)
        setvisibility(hwnd, TRUE);

    if (font)
        DeleteObject(font);
}

void
clearurgent(Client *c) {
    c->isurgent = false;
}

void
detach(Client *c) {
    Client **tc;

    for (tc = &clients; *tc && *tc != c; tc = &(*tc)->next);
    *tc = c->next;
}

void
detachstack(Client *c) {
    Client **tc;

    for (tc = &stack; *tc && *tc != c; tc = &(*tc)->snext);
    *tc = c->snext;
}

void
drawbar(Monitor *m) {
    if (!showbar || !m->barhwnd) return;

    dc.hdc = GetWindowDC(m->barhwnd);
    dc.h = m->bh;

    int x;
    unsigned int i, occ = 0, urg = 0;
    unsigned long *col;
    Client *c;
    time_t timer;
    struct tm date;
    wchar_t timestr[256];
    wchar_t localtimestr[256];
    wchar_t utctimestr[256];

    /* compute occupancy only for this monitor */
    for (c = clients; c; c = c->next) {
        if (c->mon != m) continue;
        occ |= c->tags;
        if (c->isurgent)
            urg |= c->tags;
    }

    dc.x = 0;
    for (i = 0; i < LENGTH(tags); i++) {
        dc.w = TEXTW(tags[i]);
        unsigned int cur_tagset = m->tagset[m->seltags];
        col = cur_tagset & 1 << i ? dc.sel : dc.norm;
        drawtext(tags[i], col, urg & 1 << i);
        drawsquare(sel && sel->mon == m && sel->tags & 1 << i, occ & 1 << i, urg & 1 << i, col);
        dc.x += dc.w;
    }
    if (m->blw > 0) {
        dc.w = m->blw;
        drawtext(mon_get_layout(m, m->sellt)->symbol, dc.norm, false);
        x = dc.x + dc.w;
    }
    else
        x = dc.x;
    dc.w = TEXTW(stext);
    dc.x = m->ww - dc.w;
    if (dc.x < x) {
        dc.x = x;
        dc.w = m->ww - x;
    }
    drawtext(stext, dc.norm, false);

    if (showclock) {
        /* Draw Date Time */
        timer = time(NULL);
        localtime_s(&date, &timer);
        wcsftime(localtimestr, 255, clockfmt, &date);

        if (showutcclock) {
            timer = time(NULL);
            gmtime_s(&date, &timer);
            wcsftime(utctimestr, 255, clockfmt, &date);

            swprintf(timestr, sizeof(timestr) / sizeof(timestr[0]), L"%s | UTC: %s", localtimestr, utctimestr);
        } else {
            swprintf(timestr, sizeof(timestr) / sizeof(timestr[0]), L"%s", localtimestr);
        }

        dc.w = TEXTW(timestr);
        dc.x = m->ww - dc.w;
        drawtext(timestr, dc.norm, false);
    }

    if ((dc.w = dc.x - x) > m->bh) {
        dc.x = x;
        if (sel && sel->mon == m) {
            drawtext(getclienttitle(sel->hwnd), dc.sel, false);
            drawsquare(sel->isfixed, sel->isfloating, false, dc.sel);
        }
        else
            drawtext(NULL, dc.norm, false);
    }

    ReleaseDC(m->barhwnd, dc.hdc);
}

void
drawsquare(bool filled, bool empty, bool invert, unsigned long col[ColLast]) {
    static int size = 5;
    RECT r = { .left = dc.x + 1, .top = dc.y + 1, .right = dc.x + size, .bottom = dc.y + size };

    HBRUSH brush = CreateSolidBrush(col[invert ? ColBG : ColFG]);
    SelectObject(dc.hdc, brush);

    if (filled) {
        FillRect(dc.hdc, &r, brush);
    } else if (empty) {
        FillRect(dc.hdc, &r, brush);
    }
    DeleteObject(brush);
}

void
drawtext(const wchar_t *text, unsigned long col[ColLast], bool invert) {
    RECT r = { .left = dc.x, .top = dc.y, .right = dc.x + dc.w, .bottom = dc.y + dc.h };

    HPEN pen = CreatePen(PS_SOLID, borderpx, selbordercolor);
    HBRUSH brush = CreateSolidBrush(col[invert ? ColFG : ColBG]);
    SelectObject(dc.hdc, pen);
    SelectObject(dc.hdc, brush);
    FillRect(dc.hdc, &r, brush);

    DeleteObject(brush);
    DeleteObject(pen);

    SetBkMode(dc.hdc, TRANSPARENT);
    SetTextColor(dc.hdc, col[invert ? ColBG : ColFG]);

    if (!font) {
        font = CreateFontW(fontsize, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, fontname);
        if (!font)
            font = (HFONT)GetStockObject(SYSTEM_FONT);
        }
    SelectObject(dc.hdc, font);

    DrawTextW(dc.hdc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void
eprint(bool premortem, const wchar_t *errstr, ...) {
    va_list ap;
    int num_of_chars;
    wchar_t* buffer = NULL;
    size_t buffer_num_of_chars;
    wchar_t program_name[] = L"dwm-win32: ";

    va_start(ap, errstr);

    num_of_chars = _vscwprintf(errstr, ap);
    if (num_of_chars == -1) {
        OutputDebugStringW(L"_vscwprintf failed in eprint");
        goto cleanup;
    }

    buffer_num_of_chars = wcslen(program_name) + num_of_chars + 1;
    buffer = (wchar_t*)calloc(buffer_num_of_chars, sizeof(wchar_t));
    if (buffer == NULL) {
        OutputDebugStringW(L"calloc failed in eprint");
        goto cleanup;
    }

    if (wcscpy_s(buffer, buffer_num_of_chars, program_name) != 0) {
        OutputDebugStringW(L"wcscpy_s failed in eprint");
        goto cleanup;
    }

    if (vswprintf(buffer + wcslen(program_name), num_of_chars + 1, errstr, ap) < 0) {
        OutputDebugStringW(L"vswprintf failed in eprint");
        goto cleanup;
    }

    OutputDebugStringW(buffer);

    if (premortem)
        MessageBoxW(NULL, buffer, L"dwm-win32 has encountered an error", MB_ICONERROR | MB_SETFOREGROUND | MB_OK);

cleanup:
    if (buffer != NULL)
        free(buffer);
    
    va_end(ap);
}

void
setselected(Client *c) {
    if (!c || !ISVISIBLE(c))
        for (c = stack; c && (!ISVISIBLE(c) || c->mon != selmon); c = c->snext);
    if (sel && sel != c)
        drawborder(sel, normbordercolor);
    if (c) {
        if (c->isurgent)
            clearurgent(c);
        detachstack(c);
        attachstack(c);
        drawborder(c, selbordercolor);
        selmon = c->mon;
    }
    sel = c;
    for (Monitor *m = mons; m; m = m->next) drawbar(m);
}

void
focus(Client *c) {
    setselected(c);
    if (sel)
        SetForegroundWindow(sel->hwnd);
}

void
focusstack(const Arg *arg) {
    Client *c = NULL, *i;

    if (!sel)
        return;
    if (arg->i > 0) {
        for (c = sel->next; c && (!ISFOCUSABLE(c) || c->mon != selmon); c = c->next);
        if (!c)
            for (c = clients; c && (!ISFOCUSABLE(c) || c->mon != selmon); c = c->next);
    }
    else {
        for (i = clients; i != sel; i = i->next)
            if (ISFOCUSABLE(i) && i->mon == selmon)
                c = i;
        if (!c)
            for (; i; i = i->next)
                if (ISFOCUSABLE(i) && i->mon == selmon)
                    c = i;
    }
    if (c) {
        focus(c);
        restack();
    }
}

Client *
managechildwindows(Client *p) {
    Client *c, *t;
    EnumChildWindows(p->hwnd, scan, 0);
    for (c = clients; c; ) {
        if (c->parent == p->hwnd) {
            if (!c->isalive && !IsWindowVisible(c->hwnd)) {
                t = c->next;
                unmanage(c);
                c = t;
                continue;
            }
            c->isalive = false;
        }
        c = c->next;
    }
    return nextchild(p, clients);
}

Client *
getclient(HWND hwnd) {
    Client *c;
    for (c = clients; c; c = c->next)
        if (c->hwnd == hwnd)
            return c;
    return NULL;
}

LPWSTR
getclientclassname(HWND hwnd) {
    static wchar_t buf[500];
    GetClassNameW(hwnd, buf, (int)LENGTH(buf));
    return buf;
}

LPWSTR
getclienttitle(HWND hwnd) {
    static wchar_t buf[500];
    GetWindowTextW(hwnd, buf, (int)LENGTH(buf));
    return buf;
}

HWND
getroot(HWND hwnd) {
    HWND parent, deskwnd = GetDesktopWindow();
    while ((parent = GetWindow(hwnd, GW_OWNER)) != NULL && deskwnd != parent)
        hwnd = parent;
    return hwnd;
}

void
grabkeys(HWND hwnd) {
    int i;
    for (i = 0; i < LENGTH(keys); i++) {
        RegisterHotKey(hwnd, i, keys[i].mod, keys[i].key);
    }
}

bool
iscloaked(HWND hwnd) {
    int cloaked_val;
    HRESULT h_res = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked_val, sizeof(cloaked_val));
    if (h_res != S_OK)
        cloaked_val = 0;
    return cloaked_val ? true : false;
}

bool
ismanageable(HWND hwnd) {
    if (hwnd == 0)
        return false;

    if (getclient(hwnd))
        return true;

    HWND parent = GetParent(hwnd);    
    int style = GetWindowLong(hwnd, GWL_STYLE);
    int exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    bool pok = (parent != 0 && ismanageable(parent));
    bool istool = exstyle & WS_EX_TOOLWINDOW;
    bool isapp = exstyle & WS_EX_APPWINDOW;
    bool noactiviate = exstyle & WS_EX_NOACTIVATE;
    const wchar_t *classname = getclientclassname(hwnd);
    const wchar_t *title = getclienttitle(hwnd);

    if (pok && !getclient(parent))
        manage(parent);

    /* Skip untitled or disabled */
    if (GetWindowTextLength(hwnd) == 0)
        return false;

    if (style & WS_DISABLED)
        return false;

    if (noactiviate)
        return false;

    /* Skip inactive suspended modern apps */
    if (iscloaked(hwnd))
        return false;

    if (wcsstr(classname, L"Windows.UI.Core.CoreWindow") && (
        wcsstr(title, L"Windows Shell Experience Host") ||
        wcsstr(title, L"Microsoft Text Input Application") ||
        wcsstr(title, L"Action center") ||
        wcsstr(title, L"New Notification") ||
        wcsstr(title, L"Date and Time Information") ||
        wcsstr(title, L"Volume Control") ||
        wcsstr(title, L"Network Connections") ||
        wcsstr(title, L"Cortana") ||
        wcsstr(title, L"Start") ||
        wcsstr(title, L"Windows Default Lock Screen") ||
        wcsstr(title, L"Search"))) {
        return false;
    }

    if (wcsstr(classname, L"ForegroundStaging") ||
        wcsstr(classname, L"ApplicationManager_DesktopShellWindow") ||
        wcsstr(classname, L"Static") ||
        wcsstr(classname, L"Scrollbar") ||
        wcsstr(classname, L"Progman")) {
        return false;
    }

    /* Manage either top-level visible windows or tool windows with manageable parents */
    if ((parent == 0 && IsWindowVisible(hwnd)) || pok) {
        if ((!istool && parent == 0) || (istool && pok))
            return true;
        if (isapp && parent != 0)
            return true;
    }
    return false;
}

void
killclient(const Arg *arg) {
    if (!sel)
        return;
    PostMessage(sel->hwnd, WM_CLOSE, 0, 0);
}

static void
update_client_monitor(Client *c) {
    Monitor *m = monitor_from_hwnd(c->hwnd);
    if (m && m != c->mon) {
        c->mon = m;
    }
}

Client*
manage(HWND hwnd) {    
    Client *c = getclient(hwnd);

    if (c)
        return c;

    WINDOWINFO wi = { .cbSize = sizeof(WINDOWINFO) };
    if (!GetWindowInfo(hwnd, &wi))
        return NULL;

    if (!(c = calloc(1, sizeof(Client))))
        die(L"fatal: could not calloc() %u bytes for new client\n", (unsigned)sizeof(Client));

    c->hwnd = hwnd;
    c->threadid = GetWindowThreadProcessId(hwnd, NULL);
    c->parent = GetParent(hwnd);
    c->root = getroot(hwnd);
    c->isalive = true;
    c->processname = L"";
    c->iscloaked = iscloaked(hwnd);
    c->bw = 0;

    c->mon = monitor_from_hwnd(hwnd);
    if (!c->mon) c->mon = selmon ? selmon : mons;

    GetWindowThreadProcessId(hwnd, &c->processid);
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, c->processid);
    if (hProc) {
        DWORD buf_size = MAX_PATH;
        wchar_t *buf = (wchar_t*)calloc(buf_size, sizeof(wchar_t));
        if (buf && QueryFullProcessImageNameW(hProc, 0, buf, &buf_size)) {
            c->processname = buf;
        } else {
            if (buf) free(buf);
        }
        CloseHandle(hProc);
    }

    static WINDOWPLACEMENT wp = {
        .length = sizeof(WINDOWPLACEMENT),
        .showCmd = SW_RESTORE,
    };

    if (IsWindowVisible(hwnd))
        SetWindowPlacement(hwnd, &wp);
    
    c->isfloating = (!(wi.dwStyle & WS_MINIMIZEBOX) && !(wi.dwStyle & WS_MAXIMIZEBOX));

    c->ignoreborder = iscloaked(hwnd);

    applyrules(c);


    if (c->isfloating && IsWindowVisible(hwnd)) {
        resize(c, wi.rcWindow.left, wi.rcWindow.top,
               wi.rcWindow.right - wi.rcWindow.left,
               wi.rcWindow.bottom - wi.rcWindow.top);
    }

    attach(c);
    attachstack(c);
    return c;
}

void
monocle(void) {
    for (Client *c = nexttiled(clients); c; c = nexttiled(c->next)) {
        if (c->mon != curmon) continue;
        int bw = c->bw;
        resize(c, curmon->wx, curmon->wy, curmon->ww - 2 * bw, curmon->wh - 2 * bw);
    }
}

Client *
nextchild(Client *p, Client *c) {
    for (; c && c->parent != p->hwnd; c = c->next);
    return c;
}

Client *
nexttiled(Client *c) {
    for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
    return c;
}

void
quit(const Arg *arg) {
    PostMessage(dwmhwnd, WM_CLOSE, 0, 0);
}

void
resize(Client *c, int x, int y, int w, int h) {
    if (w <= 0 && h <= 0) {
        setvisibility(c->hwnd, false);
        return;
    }

    Monitor *m = c->mon ? c->mon : selmon;

    if (!m) {
        /* fallback to system virtual screen */
        int sx = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int sy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int sw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int sh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        if (x > sx + sw) x = sw - WIDTH(c);
        if (y > sy + sh) y = sh - HEIGHT(c);
        if (x + w + 2 * c->bw < sx) x = sx;
        if (y + h + 2 * c->bw < sy) y = sy;
    } else {
        /* clamp to monitor area */
        if (x > m->sx + m->sw) x = m->sw - WIDTH(c);
        if (y > m->sy + m->sh) y = m->sh - HEIGHT(c);
        if (x + w + 2 * c->bw < m->sx) x = m->sx;
        if (y + h + 2 * c->bw < m->sy) y = m->sy;
        if (h < m->bh) h = m->bh;
        if (w < m->bh) w = m->bh;
    }

    if (c->x != x || c->y != y || c->w != w || c->h != h) {
        c->x = x;
        c->y = y;
        c->w = w;
        c->h = h;
        SetWindowPos(c->hwnd, HWND_TOP, c->x, c->y, c->w, c->h, SWP_NOACTIVATE);
    }
}

void
restack(void) {
    /* No z-order restacking required for this port */
}

LRESULT CALLBACK barhandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Monitor *m = bar_monitor_from_hwnd(hwnd);
    switch (msg) {
        case WM_CREATE:
            updatebars();
            break;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            drawbar(m);
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            buttonpress(msg, &MAKEPOINTS(lParam), m);
            break;
        case WM_TIMER:
            drawbar(m);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam); 
    }

    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_CREATE:
            break;
        case WM_CLOSE:
            cleanup();
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_HOTKEY:
            if (wParam > 0 && wParam < LENGTH(keys)) {
                keys[wParam].func(&(keys[wParam ].arg));
            }
            break;
        case WM_DISPLAYCHANGE:
        case WM_DEVICECHANGE:
            updategeom();
            updatebars();
            arrange();
            break;
        default:
            if (msg == shellhookid) { /* Handle the shell hook message */
                Client *c = getclient((HWND)lParam);
                switch (wParam & 0x7fff) {
                    case HSHELL_WINDOWCREATED:
                        if (!c && ismanageable((HWND)lParam)) {
                            c = manage((HWND)lParam);
                            managechildwindows(c);
                            arrange();
                        }
                        break;
                    case HSHELL_WINDOWDESTROYED:
                        if (c) {
                            if (!c->ignore)
                                unmanage(c);
                            else
                                c->ignore = false;
                        }
                        break;
                    case HSHELL_WINDOWACTIVATED:
                        if (c) {
                            Client *t = sel;
                            managechildwindows(c);

                            update_client_monitor(c);

                            setselected(c);

                            if (t && (t->isminimized = IsIconic(t->hwnd))) {
                                arrange();
                            }
                            if (sel && sel->isminimized) {
                                sel->isminimized = false;                                
                                zoom(NULL);
                            }
                        } else  {
                            if (ismanageable((HWND)lParam)) {
                                c = manage((HWND)lParam);
                                managechildwindows(c);
                                setselected(c);
                                arrange();
                            }
                        }
                        break;
                }
            } else
                return DefWindowProc(hwnd, msg, wParam, lParam); 
    }

    return 0;
}

void
CALLBACK
wineventproc(HWINEVENTHOOK heventhook, DWORD event, HWND hwnd, LONG object, LONG child, DWORD eventthread, DWORD eventtime_ms) {
    if (object != OBJID_WINDOW || child != CHILDID_SELF || event != EVENT_OBJECT_UNCLOAKED || hwnd == NULL)
        return;

    Client *c = getclient(hwnd);

    if (!c && ismanageable(hwnd)) {
        c = manage(hwnd);
        managechildwindows(c);
        setselected(c);
        arrange();
    }
}

BOOL CALLBACK 
scan(HWND hwnd, LPARAM lParam) {
    Client *c = getclient(hwnd);
    if (c)
        c->isalive = true;
    else if (ismanageable(hwnd))
        manage(hwnd);

    return TRUE;
}

/* Only works on Windows 11 and later */
void
drawborder(Client *c, COLORREF color) {
    if (!c || !IsWindow(c->hwnd)) return;
    DwmSetWindowAttribute(
        c->hwnd,
        34,
        &color,
        sizeof(color)
    );
}

void
setborder(Client *c, bool border) {
    if (!c->ignoreborder) {
        if (border) {
            SetWindowLong(c->hwnd, GWL_STYLE, (GetWindowLong(c->hwnd, GWL_STYLE) | (WS_CAPTION | WS_SIZEBOX)));
        } else {
            SetWindowLong(c->hwnd, GWL_STYLE, (GetWindowLong(c->hwnd, GWL_STYLE) & ~(WS_CAPTION | WS_SIZEBOX)) | WS_BORDER | WS_THICKFRAME);
            SetWindowLong(c->hwnd, GWL_EXSTYLE, (GetWindowLong(c->hwnd, GWL_EXSTYLE) & ~(WS_EX_CLIENTEDGE | WS_EX_WINDOWEDGE)));
        }
        SetWindowPos(c->hwnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER );
        c->border = border;
    }
}

void
setvisibility(HWND hwnd, bool visibility) {
    SetWindowPos(hwnd, 0, 0, 0, 0, 0, (visibility ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}

void
setlayout(const Arg *arg) {
    if (!selmon) selmon = mons; /* fallback */
    if (!arg || !arg->v || arg->v != mon_get_layout(selmon, selmon->sellt))
        selmon->sellt ^= 1;
    if (arg && arg->v)
        selmon->lt[selmon->sellt] = (Layout *)arg->v;
    if (sel)
        arrange();
    else
        updatebars();
}

void
setmfact(const Arg *arg) {
    float f;
    if (!arg || !mon_get_layout(selmon, selmon->sellt)->arrange)
        return;
    f = arg->f < 1.0 ? arg->f + mfact : arg->f - 1.0;
    if (f < 0.1 || f > 0.9)
        return;
    mfact = f;
    arrange();
}

void
setup(HINSTANCE hInstance) {
    /* initialize global fallback layouts */
    lt[0] = &layouts[0];
    lt[1] = &layouts[1 % LENGTH(layouts)];

    /* init appearance */
    dc.norm[ColBorder] = normbordercolor;
    dc.norm[ColBG] = normbgcolor;
    dc.norm[ColFG] = normfgcolor;
    dc.sel[ColBorder] = selbordercolor;
    dc.sel[ColBG] = selbgcolor;
    dc.sel[ColFG] = selfgcolor;

    /* save colors so we can restore them in cleanup */
    for (unsigned int i = 0; i < LENGTH(colorwinelements); i++)
        colors[0][i] = GetSysColor(colorwinelements[i]);
    
    SetSysColors(LENGTH(colorwinelements), colorwinelements, colors[1]); 
    
    HWND hwnd = FindWindowW(L"Shell_TrayWnd", NULL);
    if (hwnd)
        setvisibility(hwnd, showexploreronstart);

    WNDCLASSEXW winClass;
    winClass.cbSize = sizeof(WNDCLASSEXW);
    winClass.style = 0;
    winClass.lpfnWndProc = WndProc;
    winClass.cbClsExtra = 0;
    winClass.cbWndExtra = 0;
    winClass.hInstance = hInstance;
    winClass.hIcon = NULL;
    winClass.hIconSm = NULL;
    winClass.hCursor = NULL;
    winClass.hbrBackground = NULL;
    winClass.lpszMenuName = NULL;
    winClass.lpszClassName = NAME;

    if (!RegisterClassExW(&winClass))
        die(L"Error registering window class");

    dwmhwnd = CreateWindowExW(0, NAME, NAME, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);
    if (!dwmhwnd)
        die(L"Error creating window");

    /* build monitors and bars */
    buildmonitors();
    for (Monitor *m = mons; m; m = m->next) {
        setbar(hInstance, m);
    }

    /* initial scan of windows */
    EnumWindows(scan, 0);

    selmon = mons;

    grabkeys(dwmhwnd);

    arrange();
    
    if (!RegisterShellHookWindow(dwmhwnd))
        die(L"Could not RegisterShellHookWindow");

    shellhookid = RegisterWindowMessageW(L"SHELLHOOK");

    wineventhook = SetWinEventHook(EVENT_OBJECT_UNCLOAKED, EVENT_OBJECT_UNCLOAKED, NULL, wineventproc, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!wineventhook)
        die(L"Could not SetWinEventHook");

    updatebars();

    focus(NULL);
}

void
setbar(HINSTANCE hInstance, Monitor *m) {
    WNDCLASSW winClass;
    memset(&winClass, 0, sizeof winClass);

    winClass.style = 0;
    winClass.lpfnWndProc = barhandler;
    winClass.cbClsExtra = 0;
    winClass.cbWndExtra = 0;
    winClass.hInstance = hInstance;
    winClass.hIcon = NULL;
    winClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    winClass.hbrBackground = NULL;
    winClass.lpszMenuName = NULL;
    winClass.lpszClassName = L"dwm-bar";

    RegisterClassW(&winClass);

    m->barhwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        L"dwm-bar",
        NULL,
        WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 
        0, 0, 0, 0, 
        NULL,
        NULL,
        hInstance,
        NULL
    );

    m->seltags = 0;
    m->sellt = 0;
    m->tagset[0] = tagset[0];
    m->tagset[1] = tagset[1];
    m->lt[0] = &layouts[0];
    m->lt[1] = &layouts[1 % LENGTH(layouts)];

    /* calculate width of the largest layout symbol */
    dc.hdc = GetWindowDC(m->barhwnd);
    HFONT hfont = (HFONT)GetStockObject(SYSTEM_FONT); 
    SelectObject(dc.hdc, hfont);

    m->blw = 0;
    for (unsigned int i = 0; LENGTH(layouts) > 1 && i < LENGTH(layouts); i++) {
        int w = TEXTW(layouts[i].symbol);
        m->blw = MAX(m->blw, w);
    }

    ReleaseDC(m->barhwnd, dc.hdc);

    PostMessage(m->barhwnd, WM_PAINT, 0, 0);
    SetTimer(m->barhwnd, 1, clock_interval, NULL);
}

void
showclientinfo(const Arg *arg) {
    HWND hwnd = GetForegroundWindow();
    wchar_t buffer[5000];
    swprintf(buffer, sizeof(buffer)/sizeof(buffer[0]), L"ClassName:  %s\nTitle:  %s", getclientclassname(hwnd), getclienttitle(hwnd));
    MessageBoxW(NULL, buffer, L"Window class", MB_OK);
}

void
showhide(Client *c) {
    if (!c)
        return;
    if (!ISVISIBLE(c)) {
        if (IsWindowVisible(c->hwnd)) {
            c->ignore = true;
            c->wasvisible = true;
            setvisibility(c->hwnd, false);
        }
    } else {
        if (c->wasvisible) {
            setvisibility(c->hwnd, true);
        }
    }
    showhide(c->snext);
}

void
spawn(const Arg *arg) {
    ShellExecuteW(NULL, NULL, ((wchar_t **)arg->v)[0], ((wchar_t **)arg->v)[1], NULL, SW_SHOWDEFAULT);
}

void
tag(const Arg *arg) {
    Client *c;

    if (sel && arg->ui & TAGMASK) {
        sel->tags = arg->ui & TAGMASK;
        for (c = managechildwindows(sel); c; c = nextchild(sel, c->next)) {
            if (c->isfloating)
                c->tags = arg->ui & TAGMASK;
        }
        arrange();
    }
}

int
textnw(const wchar_t *text, unsigned int len) {
    SIZE size;
    GetTextExtentPoint32W(dc.hdc, text, len, &size);
    if (size.cx > 0)
        size.cx += textmargin;
    return size.cx;  
}

void
tile(void) {
    unsigned int i, n = 0;
    Client *c;

    for (c = clients; c; c = c->next) {
        if (c->mon != curmon) continue;
        if (!c->isfloating && ISVISIBLE(c)) n++;
    }
    if (n == 0)
        return;

    /* master */
    c = nexttiled(clients);
    while (c && (c->mon != curmon || c->isfloating)) c = c->next;
    if (!c) return;

    int mw = (int)(mfact * curmon->ww);
    resize(c, curmon->wx, curmon->wy, (n == 1 ? curmon->ww : mw) - 2 * c->bw, curmon->wh - 2 * c->bw);

    if (--n == 0)
        return;

    /* tile stack */
    int x = (curmon->wx + mw > c->x + c->w) ? c->x + c->w + 2 * c->bw : curmon->wx + mw;
    int y = curmon->wy;
    int w = (curmon->wx + mw > c->x + c->w) ? curmon->wx + curmon->ww - x : curmon->ww - mw;
    int h = curmon->wh / n;
    if (h < curmon->bh)
        h = curmon->wh;

    for (i = 0, c = nexttiled(c->next); c; c = nexttiled(c->next), i++) {
        if (c->mon != curmon || c->isfloating) continue;
        resize(c, x, y, w - 2 * c->bw, ((i + 1 == n)
               ? curmon->wy + curmon->wh - y - 2 * c->bw : h - 2 * c->bw));
        if (h != curmon->wh)
            y = c->y + HEIGHT(c);
    }
}

void
togglebar(const Arg *arg) {
    showbar = !showbar;
    updategeom();
    updatebars();
    arrange();
}

void
toggleborder(const Arg *arg) {
    if (!sel)
        return;
    setborder(sel, !sel->border);
}

void
toggleexplorer(const Arg *arg) {
    HWND hwnd = FindWindowW(L"Progman", L"Program Manager");
    if (hwnd)
        setvisibility(hwnd, !IsWindowVisible(hwnd));

    hwnd = FindWindowW(L"Shell_TrayWnd", NULL);
    if (hwnd)
        setvisibility(hwnd, !IsWindowVisible(hwnd));

    updategeom();
    updatebars();
    arrange();        
}

void
togglefloating(const Arg *arg) {
    if (!sel)
        return;
    sel->isfloating = !sel->isfloating || sel->isfixed;
    setborder(sel, sel->isfloating);
    if (sel->isfloating)
        resize(sel, sel->x, sel->y, sel->w, sel->h);
    arrange();
}

void
toggletag(const Arg *arg) {
    unsigned int mask;

    if (!sel)
        return;
    
    mask = sel->tags ^ (arg->ui & TAGMASK);
    if (mask) {
        sel->tags = mask;
        arrange();
    }
}

void
toggleview(const Arg *arg) {
    if (!selmon) selmon = mons;
    unsigned int mask = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);

    if (mask) {
        selmon->tagset[selmon->seltags] = mask;
        arrange();
    }
}

void
unmanage(Client *c) {
    if (c->wasvisible)
        setvisibility(c->hwnd, true);
    if (!c->isfloating)
        setborder(c, true);
    detach(c);
    detachstack(c);
    if (sel == c)
        focus(NULL);
    /* free processname buffer if dynamically assigned */
    if (c->processname && wcslen(c->processname) > 0) {
        free((void*)c->processname);
    }
    free(c);
    arrange();
}

void
updatebars(void) {
    for (Monitor *m = mons; m; m = m->next) {
        if (!m->barhwnd) continue;
        SetWindowPos(
            m->barhwnd,
            showbar ? HWND_TOPMOST : HWND_NOTOPMOST,
            m->wx,
            m->by,
            m->ww,
            m->bh,
            (showbar ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOACTIVATE | SWP_NOSENDCHANGING
        );
    }
}

void
updategeom(void) {
    buildmonitors();
    updatebars();
}

/* Monitor enumeration callback */
BOOL CALLBACK
monenumproc(HMONITOR hMon, HDC hdc, LPRECT lprc, LPARAM lParam) {
    (void)hdc; (void)lprc; (void)lParam;
    MONITORINFOEXW mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMon, (MONITORINFO*)&mi))
        return TRUE;

    Monitor *prev = NULL;
    for (Monitor *m = mons; m; prev = m, m = m->next) {
        if (m->hmon == hMon) {
            /* update existing geometry */
            m->mi = mi;
            /* prefer work area if explorer taskbar is visible */
            HWND task = FindWindowW(L"Shell_TrayWnd", NULL);
            BOOL useWork = (task && IsWindowVisible(task));
            RECT r = useWork ? mi.rcWork : mi.rcMonitor;
            m->sx = r.left;
            m->sy = r.top;
            m->sw = r.right - r.left;
            m->sh = r.bottom - r.top;
            m->bh = 20; /* fixed bar height */
            m->wx = m->sx;
            m->wy = showbar && topbar ? m->sy + m->bh : m->sy;
            m->ww = m->sw;
            m->wh = showbar ? m->sh - m->bh : m->sh;
            m->by = showbar ? (topbar ? m->wy - m->bh : m->wy + m->wh) : -m->bh;
            return TRUE;
        }
    }

    Monitor *m = (Monitor*)calloc(1, sizeof(Monitor));
    if (!m) return TRUE;
    m->hmon = hMon;
    m->mi = mi;

    HWND task = FindWindowW(L"Shell_TrayWnd", NULL);
    BOOL useWork = (task && IsWindowVisible(task));
    RECT r = useWork ? mi.rcWork : mi.rcMonitor;

    m->sx = r.left;
    m->sy = r.top;
    m->sw = r.right - r.left;
    m->sh = r.bottom - r.top;

    m->bh = 20; /* fixed bar height */

    m->wx = m->sx;
    m->wy = showbar && topbar ? m->sy + m->bh : m->sy;
    m->ww = m->sw;
    m->wh = showbar ? m->sh - m->bh : m->sh;

    m->by = showbar ? (topbar ? m->wy - m->bh : m->wy + m->wh) : -m->bh;

    m->next = NULL;

    if (!mons) {
        mons = m;
    } else {
        prev->next = m;
    }

    return TRUE;
}

void
buildmonitors(void) {
    Monitor *old = mons;
    mons = NULL;

    EnumDisplayMonitors(NULL, NULL, monenumproc, 0);

    for (Monitor *p = old; p; ) {
        Monitor *next = p->next;
        bool still_present = false;
        for (Monitor *m = mons; m; m = m->next) if (m->hmon == p->hmon) { still_present = true; break; }
        if (!still_present) {
            if (p->barhwnd) DestroyWindow(p->barhwnd);
            free(p);
        }
        p = next;
    }

    if (!selmon) selmon = mons;
}

static Monitor *
monitor_from_hwnd(HWND hwnd) {
    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    for (Monitor *m = mons; m; m = m->next) if (m->hmon == hMon) return m;
    return mons;
}

static Monitor *
monitor_from_point(POINT pt) {
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    for (Monitor *m = mons; m; m = m->next) if (m->hmon == hMon) return m;
    return mons;
}

void
view(const Arg *arg) {
    if (!selmon) selmon = mons;
    if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
        return;
    selmon->seltags ^= 1; /* toggle sel tagset for this monitor */
    if (arg->ui & TAGMASK)
        selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
    arrange();
}

void
zoom(const Arg *arg) {
    Client *c = sel;

    if (!mon_get_layout(selmon, selmon->sellt)->arrange || mon_get_layout(selmon, selmon->sellt)->arrange == monocle || (sel && sel->isfloating))
        return;
    if (c == nexttiled(clients))
        if (!c || !(c = nexttiled(c->next)))
            return;
    detach(c);
    attach(c);
    focus(c);
    arrange();
}

void
movestack(const Arg *arg) {
    Client *c = NULL, *p = NULL, *pc = NULL, *i;

    if(arg->i > 0) {
        for(c = sel->next; c && (!ISVISIBLE(c) || c->isfloating || c->mon != selmon); c = c->next);
        if(!c)
            for(c = clients; c && (!ISVISIBLE(c) || c->isfloating || c->mon != selmon); c = c->next);

    }
    else {
        for(i = clients; i != sel; i = i->next)
            if(ISVISIBLE(i) && !i->isfloating && i->mon == selmon)
                c = i;
        if(!c)
            for(; i; i = i->next)
                if(ISVISIBLE(i) && !i->isfloating && i->mon == selmon)
                    c = i;
    }
    for(i = clients; i && (!p || !pc); i = i->next) {
        if(i->next == sel)
            p = i;
        if(i->next == c)
            pc = i;
    }

    if(c && c != sel) {
        Client *temp = sel->next==c?sel:sel->next;
        sel->next = c->next==sel?c:c->next;
        c->next = temp;

        if(p && p != c)
            p->next = c;
        if(pc && pc != sel)
            pc->next = sel;

        if(sel == clients)
            clients = c;
        else if(c == clients)
            clients = sel;

        arrange();
    }
}

static void
focusmon(const Arg *arg) {
    if (!mons) return;
    if (!selmon) selmon = mons;

    Monitor *cur = selmon;
    Monitor *target = NULL;

    if (arg->i > 0) {
        if (cur->next) target = cur->next;
        else target = mons; /* wrap */
    } else {
        Monitor *prev = NULL;
        for (Monitor *t = mons; t && t != cur; t = t->next) prev = t;
        if (prev) target = prev;
        else {
            Monitor *tail = mons;
            while (tail->next) tail = tail->next;
            target = tail;
        }
    }

    if (target && target != selmon) {
        selmon = target;
        for (Client *c = stack; c; c = c->snext) {
            if (c->mon == selmon && ISFOCUSABLE(c)) {
                focus(c);
                break;
            }
        }
        /* refresh bars */
        for (Monitor *mm = mons; mm; mm = mm->next) drawbar(mm);
    }
}

static void
sendmon(const Arg *arg) {
    if (!sel) return;
    if (!mons) return;

    Monitor *cur = sel->mon ? sel->mon : selmon;
    Monitor *target = NULL;

    if (arg->i > 0) {
        if (cur->next) target = cur->next;
        else target = mons; /* wrap */
    } else {
        Monitor *prev = NULL;
        for (Monitor *t = mons; t && t != cur; t = t->next) prev = t;
        if (prev) target = prev;
        else {
            Monitor *tail = mons;
            while (tail->next) tail = tail->next;
            target = tail;
        }
    }

    if (!target || target == cur) return;

    sel->mon = target;

    SetWindowPos(sel->hwnd, HWND_TOP, target->wx, target->wy, sel->w, sel->h, SWP_NOACTIVATE | SWP_SHOWWINDOW);

    if ((sel->tags & target->tagset[target->seltags]) == 0) {
        target->tagset[target->seltags] = sel->tags & TAGMASK;
    }

    arrange();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
    (void)hPrevInstance; (void)lpCmdLine; (void)nShowCmd;

    SetProcessDPIAware();

    MSG msg;

    HANDLE mutex = CreateMutexW(NULL, TRUE, NAME);
    if (mutex == NULL)
        die(L"Failed to create dwm-win32 mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        die(L"dwm-win32 already running");

    setup(hInstance);

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    cleanup();

    return (int)msg.wParam;
}
