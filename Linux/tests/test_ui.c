/* GTK integration test: real widgets and callbacks, synthetic backend only.
 * Run through make check-ui with a working display. No privilege elevation. */
#ifdef NDEBUG
#error "This integration test requires assertions"
#endif
#include "../src/ui.h"
static const char *scenario;
static void fixture_default_size(GtkWindow *window, int width, int height);
/* Select initial geometry before the real UI presents the real GTK window. */
#define gtk_window_set_default_size fixture_default_size
#include "../src/ui.c"
#undef gtk_window_set_default_size
#include <assert.h>
#include <stdlib.h>

static system_summary_t fixture;
static const char *png_path;
static GtkApplication *preview_app;
static unsigned backend_calls;
static GdkPaintable *window_paintable;

static void fixture_default_size(GtkWindow *window, int width, int height)
{
    if (!strcmp(scenario, "narrow")) width = 560;
    gtk_window_set_default_size(window, width, height);
}

void backend_read_summary(system_summary_t *out) { backend_calls++; *out = fixture; }
int backend_is_supported(void) { return 1; }
void backend_cleanup(void) {}
char *backend_read_debug_dump(void) { return g_strdup("Synthetic preview; no hardware accessed\n"); }
void bench_run(bench_results_t *out) { (void)out; abort(); }
void pi_bench_run(int n, pi_results_t *out) { (void)n; (void)out; abort(); }

static bool screenshot(GtkWidget *window)
{
    int width = gtk_widget_get_width(window), height = gtk_widget_get_height(window);
    if (width <= 0 || height <= 0 || !gtk_widget_get_mapped(window)) return false;
    GtkSnapshot *snapshot = gtk_snapshot_new();
    gdk_paintable_snapshot(window_paintable, GDK_SNAPSHOT(snapshot), width, height);
    GskRenderNode *node = gtk_snapshot_free_to_node(snapshot);
    if (!node) return false; /* Wait for a real rendered frame, not a timer guess. */
    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, width, height);
    GdkTexture *texture = gsk_renderer_render_texture(
        gtk_native_get_renderer(GTK_NATIVE(window)), node, &bounds);
    assert(texture && gdk_texture_save_to_png(texture, png_path));
    g_object_unref(texture);
    gsk_render_node_unref(node);
    return true;
}

static gboolean capture_and_quit(gpointer unused)
{
    (void)unused;
    static unsigned attempts;
    if (!screenshot(s_widgets.window)) {
        assert(++attempts < 20 && "GTK did not produce a rendered frame within 2 seconds");
        return G_SOURCE_CONTINUE;
    }
    printf("GTK display=%s; size=%dx%d; scale=%d\n",
        G_OBJECT_TYPE_NAME(gdk_display_get_default()),
        gtk_widget_get_width(s_widgets.window), gtk_widget_get_height(s_widgets.window),
        gtk_widget_get_scale_factor(s_widgets.window));
    if (g_strcmp0(getenv("GDK_SCALE"), "2") == 0)
        assert(gtk_widget_get_scale_factor(s_widgets.window) == 2);
    printf("GTK integration %s passed; synthetic data only\n", scenario);
    g_application_quit(G_APPLICATION(preview_app));
    return G_SOURCE_REMOVE;
}

static gboolean verify_and_capture(gpointer unused)
{
    (void)unused;
    app_widgets_t *w = &s_widgets;
    if (!strcmp(scenario, "narrow")) assert(gtk_widget_get_width(w->window) <= 560);
    if (strcmp(scenario, "ddr5") == 0) {
        assert(!gtk_widget_get_visible(w->lbl_rfc4));
        assert(!gtk_widget_get_visible(w->lbl_rfc4_title));
        assert(!gtk_widget_get_visible(w->lbl_fgr));
        assert(!gtk_widget_get_visible(w->lbl_fgr_title));
        assert(gtk_widget_get_visible(w->lbl_rfcsb));
        assert(gtk_widget_get_visible(w->lbl_rfcsb_title));
    } else if (strcmp(scenario, "unknown") == 0) {
        assert(!gtk_widget_get_visible(w->lbl_rfc4));
        assert(!gtk_widget_get_visible(w->lbl_fgr));
        assert(!gtk_widget_get_visible(w->lbl_rfcsb));
        assert(!strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_tcl)), "—"));
        assert(!strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_gdm)), "—"));
        assert(!strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_powerdown)), "—"));
        assert(!strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_cmd2t)), "—"));
    } else {
        assert(gtk_widget_get_visible(w->lbl_rfc4));
        assert(gtk_widget_get_visible(w->lbl_fgr));
        assert(!gtk_widget_get_visible(w->lbl_rfcsb));
        assert(!gtk_widget_get_visible(w->lbl_rfcsb_title));
        if (strcmp(scenario, "unavailable") == 0) {
            assert(strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_rfc4)), "—") == 0);
            assert(strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_fgr)), "—") == 0);
        } else if (strcmp(scenario, "ratio") == 0) {
            assert(strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_rfc4)), "288 (— ns)") == 0);
        } else if (strcmp(scenario, "fgr5") == 0) {
            assert(strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_fgr)), "On-the-fly 1×/2×") == 0);
        } else {
            assert(strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_rfc4)), "288 (160 ns)") == 0);
            assert(strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_fgr)), "Normal 1×") == 0);
        }
    }
    if (!strcmp(scenario, "io-loss")) {
        assert(!strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_tcl)), "—"));
        assert(!strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_trp)), "16"));
        assert(!strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_gdm)), "—"));
        fixture.dram.available |= DRAM_REG_50204 | DRAM_REG_RATIO;
        refresh_ui(w);
        assert(!strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_tcl)), "16"));
        assert(!strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_gdm)), "True"));
    }
    if (!strcmp(scenario, "zero")) {
        assert(!strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_tcl)), "0"));
        assert(!strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_gdm)), "False"));
        assert(!strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_cmd2t)), "1T"));
        assert(!strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_ppt)), "0.0W"));
    }
    if (!strcmp(scenario, "lpddr5")) {
        assert(!strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_footer_type)), "LPDDR5"));
        assert(!strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_speed)), "—"));
    }
    if (!strcmp(scenario, "configured")) {
        assert(strstr(gtk_label_get_text(GTK_LABEL(w->lbl_speed)), "3600 MT/s"));
        assert(strstr(gtk_label_get_text(GTK_LABEL(w->lbl_speed)), "configured"));
        assert(strstr(gtk_label_get_text(GTK_LABEL(w->lbl_bclk)), "configured"));
    }
    if (strcmp(scenario, "temp-zero") == 0)
        assert(strstr(gtk_label_get_text(GTK_LABEL(w->lbl_spd_temp)), "jc42 1-001a: 0.00 °C"));
    if (strcmp(scenario, "temp-error") == 0)
        assert(strstr(gtk_label_get_text(GTK_LABEL(w->lbl_spd_temp)), "jc42 1-001a: Unavailable"));
    if (strcmp(scenario, "associated") == 0 || strcmp(scenario, "temp-zero") == 0) {
        const char *first = strcmp(scenario, "temp-zero") == 0 ? "0.00 °C" : "28.00 °C";
        assert(strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_module_temp)), first) == 0);
        if (strcmp(scenario, "associated") == 0) {
            unsigned before = backend_calls;
            gtk_drop_down_set_selected(GTK_DROP_DOWN(w->combo_modules), 1);
            assert(strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_module_temp)), "28.50 °C") == 0);
            assert(backend_calls == before); /* Selection must never poll hardware. */
            fixture.modules[1].temperature_available = false;
            refresh_ui(w);
            assert(strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_module_temp)), "Unavailable") == 0);
            fixture.modules[1].temperature_available = true;
            refresh_ui(w);
            assert(strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_module_temp)), "28.50 °C") == 0);
        }
    } else if (strcmp(scenario, "temp-error") == 0 || strcmp(scenario, "temp-missing") == 0) {
        assert(strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_module_temp)), "Unavailable") == 0);
    } else {
        assert(strcmp(gtk_label_get_text(GTK_LABEL(w->lbl_module_temp)), "Unassociated") == 0);
    }
    g_timeout_add(100, capture_and_quit, NULL);
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    scenario = argv[1]; png_path = argv[2];
    printf("%s; scenario=%s; synthetic backend only\n", TUXTIMINGS_BUILD_ID, scenario);
    snprintf(fixture.cpu.processor_name, STR_LEN, "Synthetic preview — Ryzen 7 5700X3D");
    snprintf(fixture.cpu.codename, STR_SHORT, "Vermeer");
    snprintf(fixture.cpu.smu_version, STR_SHORT, "56.78.0");
    snprintf(fixture.cpu.pm_table_version, STR_SHORT, "PM table 0x00380905");
    snprintf(fixture.board.display_line, sizeof(fixture.board.display_line), "Synthetic fixtures — no hardware accessed");
    fixture.memory.type = MEM_DDR4; fixture.memory.frequency = 3600;
    fixture.memory.frequency_info = (value_info_t){true,VALUE_SOURCE_SMN,VALUE_UNIT_MTS};
    fixture.memory.type_state = MEMORY_TYPE_CONSISTENT;
    snprintf(fixture.memory.total_capacity, sizeof(fixture.memory.total_capacity), "16.0 GiB");
    fixture.module_count = 2;
    for (int i=0; i<2; i++) {
        memory_module_t *m = &fixture.modules[i];
        m->type = MEM_DDR4;
        m->temp_state = DIMM_TEMP_IDENTITY_MISSING;
        snprintf(m->slot_display, sizeof(m->slot_display), "Module %d: DIMM_%c2 - 8.0 GiB", i+1, 'A'+i);
        snprintf(m->capacity_display, sizeof(m->capacity_display), "8.0 GiB");
        snprintf(m->manufacturer, STR_LEN, "G.Skill");
        snprintf(m->part_number, STR_LEN, "F4-4133C19-8GTZC");
    }
    fixture.metrics.mclk_mhz = fixture.metrics.fclk_mhz = fixture.metrics.uclk_mhz = 1800;
    fixture.metrics.bclk_mhz=100;
    fixture.metrics.bclk_info=(value_info_t){true,VALUE_SOURCE_MSR_ESTIMATE,VALUE_UNIT_MHZ};
    fixture.metrics.spd_temps_count = 2;
    fixture.metrics.spd_temps_available[0] = fixture.metrics.spd_temps_available[1] = true;
    if (strcmp(scenario,"associated")==0 || strncmp(scenario,"temp-",5)==0) {
        for (int i=0;i<2;i++) {
            fixture.modules[i].temp_state = DIMM_TEMP_ASSOCIATED;
            fixture.modules[i].temperature_available = true;
            fixture.modules[i].temperature_c = 28.0f + i*0.5f;
            snprintf(fixture.modules[i].temperature_sensor, STR_SHORT, "jc42 1-001%c", 'a'+i);
        }
        if (strcmp(scenario,"temp-zero")==0) fixture.modules[0].temperature_c=0.0f;
        if (strcmp(scenario,"temp-error")==0) fixture.modules[0].temperature_available=false;
        if (strcmp(scenario,"temp-missing")==0) fixture.modules[0].temp_state=DIMM_TEMP_SENSOR_MISSING;
        if (strcmp(scenario,"temp-ambiguous")==0) fixture.modules[0].temp_state=DIMM_TEMP_AMBIGUOUS;
    }
    fixture.metrics.spd_temps_c[0]=28.0; fixture.metrics.spd_temps_c[1]=28.5;
    if (strcmp(scenario,"temp-zero")==0) fixture.metrics.spd_temps_c[0]=0.0f;
    if (strcmp(scenario,"temp-error")==0) fixture.metrics.spd_temps_available[0]=false;
    snprintf(fixture.metrics.spd_temp_labels[0], STR_SHORT, "jc42 1-001a");
    snprintf(fixture.metrics.spd_temp_labels[1], STR_SHORT, "jc42 1-001b");
    dram_timings_t *d=&fixture.dram;
    d->layout=DRAM_LAYOUT_DDR4;
    d->available = (1u << 20) - 1;
    d->data_rate_mts = 3600;
    d->data_rate_info = (value_info_t){true,VALUE_SOURCE_SMN,VALUE_UNIT_MTS};
    d->ddr4_fgr_io_ok=d->ddr4_rfc_available=d->ddr4_ratio_available=true;
    d->tcl=d->trcd_rd=d->trcd_wr=d->trp=16; d->tras=36; d->trc=52;
    d->rfc=630; d->rfc2=468; d->rfc4=288;
    d->trfc_ns=350; d->trfc2_ns=260; d->trfc4_ns=160;
    d->refi=14029; d->trefi_ns=7794; d->gdm_enabled=true;
    snprintf(d->cmd2t,sizeof(d->cmd2t),"1T");
    if (strcmp(scenario,"ddr5")==0) {
        d->layout=DRAM_LAYOUT_DDR5; d->rfcsb=160;
        fixture.memory.type=MEM_DDR5; fixture.memory.frequency=6000;
    } else if (strcmp(scenario,"unknown")==0) d->layout=DRAM_LAYOUT_NONE;
    else if (strcmp(scenario,"unavailable")==0) {
        d->ddr4_fgr_io_ok=false; d->ddr4_rfc_available=false;
    } else if (strcmp(scenario,"ratio")==0) {
        d->ddr4_ratio_available=false; d->data_rate_info.available=false;
    }
    else if (strcmp(scenario,"fgr5")==0) d->fgr=5;
    else if (!strcmp(scenario,"io-loss")) d->available &= ~(DRAM_REG_50204 | DRAM_REG_RATIO);
    else if (!strcmp(scenario,"zero")) {
        d->tcl=0; d->gdm_enabled=false; fixture.metrics.ppt_available=true;
    } else if (!strcmp(scenario,"lpddr5")) {
        fixture.memory.type=MEM_LPDDR5;
        fixture.memory.frequency_info.available=false;
    } else if (!strcmp(scenario,"configured")) {
        fixture.memory.frequency_info.source=VALUE_SOURCE_SMBIOS;
        fixture.metrics.bclk_info.source=VALUE_SOURCE_SMBIOS;
    }
    else if (!strcmp(scenario,"long")) {
        memset(fixture.modules[0].part_number,'X',STR_LEN-1);
        memset(fixture.board.display_line,'W',sizeof(fixture.board.display_line)-1);
        memset(fixture.cpu.processor_name,'C',STR_LEN-1);
    }
    preview_app=ui_create(0,NULL);
    g_application_set_flags(G_APPLICATION(preview_app), G_APPLICATION_NON_UNIQUE);
    GError *error=NULL;
    assert(g_application_register(G_APPLICATION(preview_app),NULL,&error));
    g_application_activate(G_APPLICATION(preview_app));
    window_paintable=gtk_widget_paintable_new(s_widgets.window);
    g_timeout_add(400,verify_and_capture,NULL);
    int rc=g_application_run(G_APPLICATION(preview_app),0,NULL);
    g_object_unref(window_paintable);
    g_object_unref(preview_app);
    return rc;
}
