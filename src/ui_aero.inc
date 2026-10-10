/* Included by ui.c to share the exact Aero palette and panel renderer. */
static void wrapped(float x, float y, const char *text, int width, int rows, int skip,
                    uint32_t color) {
    text = L(text);
    int line = 0;
    while (*text && line < rows + skip) {
        char buffer[CL_EXPR];
        int end[CL_EXPR], count = 0, bytes = 0;
        const char *p = text;
        while (*p && bytes < CL_EXPR - 5) {
            unsigned char c = (unsigned char)*p;
            int n = c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : c >= 0xc0 ? 2 : 1;
            for (int j = 0; j < n && *p; j++) {
                buffer[bytes++] = *p++;
            }
            end[count++] = bytes;
        }
        buffer[bytes] = 0;
        int low = 0, high = count;
        while (low < high) {
            int mid = low + (high - low + 1) / 2;
            int at = end[mid - 1];
            char saved = buffer[at];
            buffer[at] = 0;
            int fits = gfx_text_width(buffer, 1) <= width;
            buffer[at] = saved;
            if (fits)
                low = mid;
            else
                high = mid - 1;
        }
        int n = low ? end[low - 1] : 0;
        if (!n) {
            text = p;
            continue;
        }
        int next = n;
        if (text[n]) {
            for (int i = n - 1; i > 0; i--)
                if (buffer[i] == ' ') {
                    n = i;
                    next = i + 1;
                    break;
                }
        }
        buffer[n] = 0;
        if (line >= skip)
            gfx_text(x, y + (line - skip) * 14, buffer, 1, color);
        text += next;
        while (*text == ' ')
            text++;
        line++;
    }
}
static void report_lines(const CLToolResult *r, int scroll, Palette p) {
    panel(11, 74, 458, 166, p);
    for (int row = 0; row < 10 && row + scroll < r->count; ++row)
        fit(17, 79 + row * 14, r->line[row + scroll], 73, 1, r->ok ? p.ink : p.error);
    char s[80];
    snprintf(s, sizeof(s), "Lines %d-%d / %d", r->count ? scroll + 1 : 0,
             scroll + 10 < r->count ? scroll + 10 : r->count, r->count);
    gfx_text(17, 225, s, 1, p.muted);
}
static void lab(const CLApp *a, Palette p) {
    if (a->lab_mode == 0) {
        int start = (a->lab_tool / 7) * 7;
        for (int row = 0; row < 7 && start + row < AERO_LABS; ++row) {
            int t = start + row, y = 45 + row * 22;
            glass_row(11, y, 458, 20, t == a->lab_tool, p);
            char s[160];
            snprintf(s, sizeof(s), "%02d  %s", t + 1, aero_lab_name(t));
            fit(18, y + 2, s, 73, 1, t == a->lab_tool ? p.accent : p.ink);
        }
        if (a->lab_tool == AE_FORMULA) {
            char s[160];
            snprintf(s, sizeof(s), "%02d: %s", a->lab_variant, aero_formula_name(a->lab_variant));
            fit(15, 205, s, 73, 1, p.accent);
            fit(15, 220, cl_lab_example(a), 73, 1, p.muted);
        } else
            wrapped(15, 204, aero_lab_hint(a->lab_tool), 450, 2, 0, p.muted);
        footer("Up/down: tool  X: open example",
               a->lab_tool == AE_FORMULA ? "L/R: formula  /\\: menu"
                                         : "L/R: catalog pages  /\\: menu",
               p);
        return;
    }
    fit(14, 42, aero_lab_name(a->lab_tool), 73, 1, p.accent);
    if (a->lab_mode == 2) {
        fit(14, 57, a->input[3], 73, 1, p.muted);
        report_lines(&a->lab_result, a->lab_scroll, p);
        footer("Up/down: report  X/O: return to input",
               "SELECT: catalog  /\\: menu and result export", p);
        return;
    }
    wrapped(14, 58,
            a->lab_tool == AE_FORMULA ? aero_formula_variables(a->lab_variant)
                                      : aero_lab_hint(a->lab_tool),
            450, 2, 0, p.muted);
    panel(11, 87, 458, 35, p);
    editor(a, a->input[3], 91, 2, p);
    fit(14, 123, cl_keypage_name(a->keypage), 73, 1, p.muted);
    keypad(a, 136, p);
    footer("START: run  SELECT: catalog  []: cursor", "L/R: keys  /\\ -> PSP keyboard", p);
}
static void matrix_editor(const CLApp *a, Palette p) {
    char s[160];
    int n = a->extra.matrix_n;
    snprintf(s, sizeof(s), "MATRIX %c: %d x %d     %s", 'A' + a->matrix_slot, n, n,
             aero_matrix_name(a->matrix_op));
    fit(14, 43, s, 73, 1, p.accent);
    if (a->matrix_show) {
        report_lines(&a->matrix_result, a->matrix_scroll, p);
        footer("Up/down: report  X/O: return to matrix", "/\\: menu and result export", p);
        return;
    }
    if (a->matrix_editing) {
        snprintf(s, sizeof(s), "Cell [%d;%d]; expressions and fractions allowed", a->matrix_row + 1,
                 a->matrix_col + 1);
        fit(14, 60, s, 73, 1, p.muted);
        panel(11, 82, 458, 39, p);
        editor(a, a->input[4], 87, 2, p);
        fit(14, 123, cl_keypage_name(a->keypage), 73, 1, p.muted);
        keypad(a, 136, p);
        footer("START: save cell  SELECT: cancel", "L/R: keys  []: cursor  /\\: PSP keyboard", p);
        return;
    }
    snprintf(s, sizeof(s), "Selected [%d;%d] = %.15g", a->matrix_row + 1, a->matrix_col + 1,
             a->extra.matrix[a->matrix_slot][a->matrix_row * 4 + a->matrix_col]);
    fit(14, 60, s, 73, 1, p.muted);
    float cw = 440.0f / n, ch = 148.0f / n;
    for (int row = 0; row < n; ++row)
        for (int col = 0; col < n; ++col) {
            float x = 20 + cw * col, y = 79 + ch * row;
            int selected = row == a->matrix_row && col == a->matrix_col;
            glass_row(x, y, cw - 4, ch - 4, selected, p);
            snprintf(s, sizeof(s), "%.12g", a->extra.matrix[a->matrix_slot][row * 4 + col]);
            fit(x + 5, y + (ch - 18) / 2, s, (int)((cw - 14) / 6), 1, selected ? p.accent : p.ink);
        }
    footer("Arrows: cell  X: edit  []: size  SELECT: A/B",
           "L/R: operation  START: result  /\\: menu", p);
}
static void study(const CLApp *a, Palette p) {
    static const char *names[] = {"Fractions",   "Linear equations", "Trigonometry / DEG",
                                  "Derivatives", "Determinants",     "Probability"};
    fit(14, 43, names[a->extra.study_topic % AERO_STUDY_TOPICS], 45, 1, p.accent);
    char s[256];
    snprintf(s, sizeof(s), "%u/%u", (unsigned)a->extra.study_correct,
             (unsigned)a->extra.study_total);
    fit(325, 43, s, 23, 1, p.muted);
    wrapped(14, 62, a->study_question, 450, 2, 0, p.ink);
    if (a->study_graded) {
        gfx_text(17, 98, a->study_right ? "Correct!" : "Incorrect answer", 2,
                 a->study_right ? p.accent : p.error);
        char answer[130];
        cl_format(a->study_answer, 15, 0, 1, answer, sizeof(answer));
        snprintf(s, sizeof(s), "Correct answer: %s", answer);
        fit(17, 128, s, 73, 1, p.ink);
        wrapped(17, 150, a->study_explanation, 442, 5, 0, p.muted);
        footer("X/[]: next question  O: retry this answer", "SELECT: next topic  /\\: menu", p);
        return;
    }
    panel(11, 90, 458, 32, p);
    editor(a, a->input[5], 94, 2, p);
    fit(14, 123, cl_keypage_name(a->keypage), 73, 1, p.muted);
    keypad(a, 136, p);
    footer("START: grade  SELECT: topic  []: cursor", "L/R: keys  /\\: menu and PSP keyboard", p);
}
static void formula_view(const CLApp *a, Palette p) {
    panel(11, 42, 458, 198, p);
    CLProgram program;
    CLError e;
    fit(17, 47, "FORMULA / TEXTBOOK NOTATION", 73, 1, p.accent);
    if (!aero_compile(a->save.expr, &a->extra, &program, &e)) {
        wrapped(17, 78, cl_error_text(e.code), 442, 4, 0, p.error);
    } else if (!aero_draw_math(&program, 18, 69, 442, 161, p.ink)) {
        fit(17, 69, "Long formula: multiline source view", 73, 1, p.muted);
        wrapped(17, 86, a->save.expr, 442, 10, a->view, p.ink);
    }
    footer("[]: edit formula  START: calculator", "Up/down: source lines  /\\: menu", p);
}
