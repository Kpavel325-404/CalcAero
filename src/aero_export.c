#include "app.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __PSP__
#include <pspdisplay.h>
#include "l10n.h"
#endif
typedef struct {
    FILE *f;
    char path[800];
} Output;
static int open_output(const char *dir, const char *kind, const char *ext, Output *o, char *name,
                       size_t cap) {
    char folder[700];
    int n = snprintf(folder, sizeof(folder), "%s/reports", dir);
    struct stat info;
    if (n < 0 || (size_t)n >= sizeof(folder) || !cap)
        return 0;
    if (stat(folder, &info) == 0) {
        if (!S_ISDIR(info.st_mode))
            return 0;
    } else if (mkdir(folder, 0777) != 0 && (stat(folder, &info) != 0 || !S_ISDIR(info.st_mode)))
        return 0;
    for (int i = 1; i <= 9999; ++i) {
        n = snprintf(name, cap, "CalcAero-%s-%04d.%s", kind, i, ext);
        if (n < 0 || (size_t)n >= cap)
            return 0;
        n = snprintf(o->path, sizeof(o->path), "%s/%s", folder, name);
        if (n < 0 || (size_t)n >= sizeof(o->path))
            return 0;
        /* PSP may return a different errno for an existing file. Check the
         * candidate first, as the legacy TXT exporter does, and retain the
         * exclusive flag to protect the final create operation. */
        if (stat(o->path, &info) == 0)
            continue;
        int fd = open(o->path, O_WRONLY | O_CREAT | O_EXCL, 0666);
        if (fd < 0) {
            if (errno == EEXIST || stat(o->path, &info) == 0)
                continue;
            return 0;
        }
        o->f = fdopen(fd, "wb");
        if (o->f)
            return 1;
        close(fd);
        remove(o->path);
        return 0;
    }
    return 0;
}
static int finish(Output *o) {
    int ok = !ferror(o->f) && fflush(o->f) == 0;
    if (fclose(o->f))
        ok = 0;
    if (!ok)
        remove(o->path);
    return ok;
}
static void csv_text(FILE *f, const char *text) {
    fputc('"', f);
    for (; *text; ++text) {
        if (*text == '"')
            fputc('"', f);
        fputc(*text, f);
    }
    fputc('"', f);
}
static void xml_text(FILE *f, const char *text) {
    for (; *text; ++text) {
        switch (*text) {
        case '&':
            fputs("&amp;", f);
            break;
        case '<':
            fputs("&lt;", f);
            break;
        case '>':
            fputs("&gt;", f);
            break;
        case '"':
            fputs("&quot;", f);
            break;
        default:
            fputc(*text, f);
        }
    }
}
int aero_export_csv(const char *dir, const CLApp *a, char *name, size_t cap) {
    Output o;
    const char *kind = a->page == PAGE_GRAPH    ? "Graph"
                       : a->page == PAGE_MATRIX ? "Matrix"
                                                : "History";
    if (!open_output(dir, kind, "csv", &o, name, cap))
        return 0;
    FILE *f = o.f;
    if (a->page == PAGE_GRAPH) {
        fputs("x", f);
        for (int k = 0; k < 3; ++k) {
            char column[CL_EXPR + 16];
            snprintf(column, sizeof(column), "G%d: %s", k + 1, cl_graph_expression(a, k));
            fputc(',', f);
            csv_text(f, column);
        }
        fputc('\n', f);
        for (int i = 0; i < a->graph_index; ++i) {
            double x = a->xmin + (a->xmax - a->xmin) * i / (CL_GRAPH_POINTS - 1);
            fprintf(f, "%.17g", x);
            for (int k = 0; k < 3; ++k) {
                int valid = k ? a->other_valid[k - 1][i] : a->graph_valid[i];
                double y = k ? a->other_y[k - 1][i] : a->graph_y[i];
                fputc(',', f);
                if (valid)
                    fprintf(f, "%.17g", y);
            }
            fputc('\n', f);
        }
    } else if (a->page == PAGE_MATRIX) {
        int n = a->extra.matrix_n;
        fputs("row", f);
        for (int j = 0; j < n; ++j)
            fprintf(f, ",%c%d", 'A' + a->matrix_slot, j + 1);
        fputc('\n', f);
        for (int row = 0; row < n; ++row) {
            fprintf(f, "%d", row + 1);
            for (int col = 0; col < n; ++col)
                fprintf(f, ",%.17g", a->extra.matrix[a->matrix_slot][row * 4 + col]);
            fputc('\n', f);
        }
    } else {
        fputs("record,angle,complex,expression,result,exact\n", f);
        for (int i = 0; i < a->save.history_count; ++i) {
            const CLHistory *h = &a->save.history[i];
            char value[160];
            cl_format(h->value, 15, 0, 1, value, sizeof(value));
            fprintf(f, "%d,%s,%d,", i + 1,
                    h->angle == CL_DEG   ? "DEG"
                    : h->angle == CL_RAD ? "RAD"
                                         : "GRAD",
                    h->complex_mode);
            csv_text(f, h->expr);
            fputc(',', f);
            csv_text(f, value);
            fprintf(f, ",%d\n", h->value.exact);
        }
    }
    return finish(&o);
}
int aero_export_svg(const char *dir, const CLApp *a, char *name, size_t cap) {
    if (a->page != PAGE_GRAPH)
        return 0;
    Output o;
    if (!open_output(dir, "Graph", "svg", &o, name, cap))
        return 0;
    FILE *f = o.f;
    const double left = 35, right = 463, top = 66, bottom = 232, w = right - left, h = bottom - top;
    const char *colors[] = {"#00699d", "#148142", "#c0511c"};
    fputs(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "width=\"480\" height=\"272\" viewBox=\"0 0 480 272\">\n<rect width=\"480\" height=\"272\" "
        "fill=\"white\"/>\n<g font-family=\"monospace\" font-size=\"10\" fill=\"#18364d\">\n",
        f);
    fputs("<desc>", f);
    for (int k = 0; k < 3; ++k) {
        fprintf(f, "G%d: ", k + 1);
        xml_text(f, cl_graph_expression(a, k));
        fputc(';', f);
    }
    fputs("</desc>\n", f);
    for (int k = 0; k < 3; ++k) {
        char label[64];
        const char *expression = cl_graph_expression(a, k);
        size_t length = strlen(expression);
        if (length > 55) {
            length = 55;
            while (length && ((unsigned char)expression[length] & 0xc0) == 0x80)
                --length;
        }
        memcpy(label, expression, length);
        label[length] = 0;
        fprintf(f, "<text x=\"14\" y=\"%d\" fill=\"%s\">G%d: ", 18 + k * 14, colors[k], k + 1);
        xml_text(f, label);
        fputs("</text>\n", f);
    }
    fprintf(f,
            "<rect x=\"%.0f\" y=\"%.0f\" width=\"%.0f\" height=\"%.0f\" fill=\"#f6fcff\" "
            "stroke=\"#7db1cc\"/>\n",
            left, top, w, h);
    for (int i = 1; i < 8; ++i)
        fprintf(f, "<path d=\"M %.4f %.0f V %.0f\" stroke=\"#d7e9f5\"/>\n", left + w * i / 8, top,
                bottom);
    for (int i = 1; i < 4; ++i)
        fprintf(f, "<path d=\"M %.0f %.4f H %.0f\" stroke=\"#d7e9f5\"/>\n", left, top + h * i / 4,
                right);
    if (a->xmin <= 0 && a->xmax >= 0) {
        double x = left + w * (-a->xmin) / (a->xmax - a->xmin);
        fprintf(f, "<path d=\"M %.4f %.0f V %.0f\" stroke=\"#607080\"/>\n", x, top, bottom);
    }
    if (a->ymin <= 0 && a->ymax >= 0) {
        double y = bottom - h * (-a->ymin) / (a->ymax - a->ymin);
        fprintf(f, "<path d=\"M %.0f %.4f H %.0f\" stroke=\"#607080\"/>\n", left, y, right);
    }
    for (int k = 0; k < 3; ++k) {
        const int *valid = k ? a->other_valid[k - 1] : a->graph_valid;
        const double *values = k ? a->other_y[k - 1] : a->graph_y;
        for (int i = 1; i < a->graph_index; ++i) {
            if (!valid[i - 1] || !valid[i])
                continue;
            double y1 = bottom - h * (values[i - 1] - a->ymin) / (a->ymax - a->ymin),
                   y2 = bottom - h * (values[i] - a->ymin) / (a->ymax - a->ymin);
            if (!isfinite(y1) || !isfinite(y2) || fabs(y1 - y2) > h * 1.5 ||
                (y1 < top && y2 < top) || (y1 > bottom && y2 > bottom))
                continue;
            y1 = fmax(top, fmin(bottom, y1));
            y2 = fmax(top, fmin(bottom, y2));
            fprintf(f,
                    "<path d=\"M %.4f %.4f L %.4f %.4f\" stroke=\"%s\" stroke-width=\"1.5\" "
                    "fill=\"none\"/>\n",
                    left + w * (i - 1) / (CL_GRAPH_POINTS - 1), y1,
                    left + w * i / (CL_GRAPH_POINTS - 1), y2, colors[k]);
        }
    }
    fprintf(f,
            "<text x=\"35\" y=\"246\">x=%.8g</text><text x=\"463\" y=\"246\" "
            "text-anchor=\"end\">x=%.8g</text>\n",
            a->xmin, a->xmax);
    fprintf(f,
            "<text x=\"35\" y=\"263\">y=[%.8g; %.8g]</text><text x=\"463\" y=\"263\" "
            "text-anchor=\"end\">CalcAero 1.2</text>\n</g></svg>\n",
            a->ymin, a->ymax);
    return finish(&o);
}
int aero_export_report(const char *dir, const CLApp *a, char *name, size_t cap) {
    const CLToolResult *r = NULL;
    const char *title = "";
    if (a->page == PAGE_LAB && a->lab_mode == 2) {
        r = &a->lab_result;
        title = aero_lab_name(a->lab_tool);
    } else if (a->page == PAGE_TOOLS) {
        r = &a->tool_result;
        title = cl_tool_name(a->tool);
    } else if (a->page == PAGE_MATRIX && a->matrix_show) {
        r = &a->matrix_result;
        title = aero_matrix_name(a->matrix_op);
    }
    if (!r || !r->count)
        return 0;
    Output o;
    if (!open_output(dir, "Report", "txt", &o, name, cap))
        return 0;
    fprintf(o.f, "CalcAero 1.3.1\n%s\nAngle mode: %s\n\n", title,
            a->save.ctx.angle == CL_DEG   ? "DEG"
            : a->save.ctx.angle == CL_RAD ? "RAD"
                                          : "GRAD");
    if (a->page != PAGE_MATRIX)
        fprintf(o.f, "Input: %s\n\n", a->page == PAGE_LAB ? a->input[3] : a->input[0]);
    for (int i = 0; i < r->count; ++i)
        fprintf(o.f, "%s\n", r->line[i]);
    return finish(&o);
}
#ifdef __PSP__
static void le32(unsigned char *b, uint32_t v) {
    for (int i = 0; i < 4; ++i)
        b[i] = (unsigned char)(v >> (8 * i));
}
#endif
int aero_capture_bmp(const char *dir, char *name, size_t cap) {
#ifdef __PSP__
    void *frame = NULL;
    int stride = 0, format = 0;
    if (sceDisplayGetFrameBuf(&frame, &stride, &format, PSP_DISPLAY_SETBUF_IMMEDIATE) < 0 ||
        !frame || stride < 480 || format != PSP_DISPLAY_PIXEL_FORMAT_8888)
        return 0;
    Output o;
    if (!open_output(dir, "Screen", "bmp", &o, name, cap))
        return 0;
    unsigned char header[54] = {0}, row[480 * 3];
    header[0] = 'B';
    header[1] = 'M';
    le32(header + 2, 54 + sizeof(row) * 272);
    le32(header + 10, 54);
    le32(header + 14, 40);
    le32(header + 18, 480);
    le32(header + 22, 272);
    header[26] = 1;
    header[28] = 24;
    le32(header + 34, sizeof(row) * 272);
    fwrite(header, 1, sizeof(header), o.f);
    const uint32_t *pixels = (const uint32_t *)frame;
    for (int y = 271; y >= 0; --y) {
        for (int x = 0; x < 480; ++x) {
            uint32_t pixel = pixels[y * stride + x];
            row[x * 3] = (unsigned char)(pixel >> 16);
            row[x * 3 + 1] = (unsigned char)(pixel >> 8);
            row[x * 3 + 2] = (unsigned char)pixel;
        }
        fwrite(row, 1, sizeof(row), o.f);
    }
    return finish(&o);
#else
    (void)dir;
    (void)name;
    (void)cap;
    return 0;
#endif
}
