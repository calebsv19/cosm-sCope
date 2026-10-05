#include "render/render_view_internal.h"
#include "render/render_view_picker_interaction.h"
#include "render/render_view_picker_support.h"
#include "kit_ui_interaction_sdl.h"
#include "app/datalab_runtime_prefs.h"
#include <stdio.h>
#include <string.h>
int datalab_picker_refresh_catalog(DatalabInputCatalog *catalog,
                                          DatalabCatalogView *view,
                                          const char *root,
                                          DatalabInputCatalogRefreshReason reason,
                                          char *status,
                                          size_t status_cap) {
    CoreResult result;
    if (!catalog || !view || !root) {
        return 0;
    }
    result = datalab_input_catalog_refresh(catalog, root, reason);
    if (result.code != CORE_OK) {
        DatalabSupportedFileScanResult failed_scan = {0};
        failed_scan.root_unavailable = result.code == CORE_ERR_IO;
        failed_scan.allocation_failed = result.code == CORE_ERR_OUT_OF_MEMORY;
        failed_scan.invalid_request = result.code == CORE_ERR_INVALID_ARG;
        datalab_format_supported_file_scan_status(&failed_scan, root, "choose folder", status, status_cap);
        return 0;
    }
    datalab_catalog_view_bind(view, catalog);
    if (status && status_cap > 0u) {
        snprintf(status,
                 status_cap,
                 "found %zu supported files [catalog %s %lluus]",
                 catalog->file_count,
                 datalab_input_catalog_refresh_reason_name(reason),
                 (unsigned long long)catalog->last_scan_duration_us);
    }
    fprintf(stderr,
            "datalab: input catalog refresh reason=%s root=%s duration_us=%llu files=%zu result=%d\n",
            datalab_input_catalog_refresh_reason_name(reason),
            root,
            (unsigned long long)catalog->last_scan_duration_us,
            catalog->file_count,
            (int)result.code);
    return 1;
}

void datalab_picker_view_apply_filter(DatalabCatalogView *view,
                                             const char *filter,
                                             uint64_t *out_count) {
    if (!view) return;
    datalab_catalog_view_set_filter(view, filter);
    /* Per-event budget; subsequent frames continue truthfully scanning. */
    datalab_catalog_view_step_filter(view, 512u);
    if (out_count) *out_count = datalab_catalog_view_count(view);
}


void datalab_picker_collect_buttons(KitUiSurface *s, int w, int h,
    const DatalabPickerRecentUiState *recent, char roots[][DATALAB_APP_PATH_CAP], size_t root_count,
    const DatalabPickerRecentFileUiState *files_ui, char files[][DATALAB_APP_PATH_CAP], size_t file_count) {
    kit_ui_surface_begin(s,1u);
    KitRenderRect viewport={0,0,w,h};
    KitUiInteractionControl control;
    for (size_t i=0;i<recent->visible_count;++i) {
        size_t index=recent->first_index+i;
        if (index>=root_count) break;
        SDL_Rect r=recent->item_rects[i];
        SDL_Rect clipped, output={0,0,w,h};
        if (!SDL_IntersectRect(&recent->list_rect,&output,&clipped)) continue;
        KitRenderRect clip={clipped.x,clipped.y,clipped.w,clipped.h};
        (void)kit_ui_surface_register(s,(KitUiSurfaceKey){DATALAB_PICKER_ROOT,datalab_ui_path_key(roots[index])},
            (KitRenderRect){r.x,r.y,r.w,r.h},&clip,1,&control);
    }
    for (size_t i=0;i<files_ui->visible_count && i<file_count;++i) {
        SDL_Rect r=files_ui->item_rects[i];
        SDL_Rect clipped;
        if (!SDL_IntersectRect(&r,&files_ui->list_rect,&clipped)) continue;
        (void)kit_ui_surface_register(s,(KitUiSurfaceKey){DATALAB_PICKER_FILE,datalab_ui_path_key(files[i])},
            (KitRenderRect){clipped.x,clipped.y,clipped.w,clipped.h},&viewport,1,&control);
    }
    CoreResult result=kit_ui_surface_end(s);
    if (result.code!=CORE_OK) fprintf(stderr,"datalab picker surface: %s\n",result.message);
}
int datalab_picker_route_buttons(KitUiSurface *s, SDL_Window *window, SDL_Renderer *renderer,
    const SDL_Event *e, int editing, KitUiSurfaceKey *out) {
    *out=(KitUiSurfaceKey){0};
    if (e->type==SDL_MOUSEWHEEL || (e->type==SDL_WINDOWEVENT &&
        (e->window.event==SDL_WINDOWEVENT_RESIZED || e->window.event==SDL_WINDOWEVENT_SIZE_CHANGED))) {
        kit_ui_surface_begin(s,1u); (void)kit_ui_surface_end(s); return 0;
    }
    if (editing && e->type==SDL_KEYDOWN) {
        s->interaction.focused_id=0u; s->interaction.key_owner_id=0u; return 0;
    }
    KitUiInteractionEvent event;
    if (!kit_ui_interaction_event_from_sdl(e,&event)) return 0;
    if (event.type>=KIT_UI_INTERACTION_POINTER_MOVE && event.type<=KIT_UI_INTERACTION_POINTER_UP) {
        int x,y;
        if (!datalab_render_map_window_to_renderer_point(window,renderer,event.x,event.y,&x,&y)) return 1;
        event.x=x; event.y=y;
    }
    /* Republish the existing paint snapshot once per direct event so its
     * activation budget resets without changing geometry or semantic IDs. */
    kit_ui_surface_begin(s,1u);
    KitUiInteractionControl control;
    for (uint32_t i=0;i<s->count;++i)
        (void)kit_ui_surface_register(s,s->keys[i],s->controls[i].bounds,NULL,s->controls[i].enabled,&control);
    if (kit_ui_surface_end(s).code!=CORE_OK) return 1;
    KitUiInteractionResult result;
    if (kit_ui_surface_route(s,&event,&result).code!=CORE_OK) return 1;
    if (result.activated_id && kit_ui_surface_take_activation(s,result.activated_id))
        (void)kit_ui_surface_key(s,result.activated_id,out);
    return event.type==KIT_UI_INTERACTION_CANCEL ? 0:result.consumed;
}
void datalab_picker_draw_row(KitUiSurface *s, SDL_Renderer *renderer, SDL_Rect r,
    uint32_t domain, const char *path, int selected, const DatalabPickerThemePalette *palette) {
    KitUiInteractionControl c={0,{r.x,r.y,r.w,r.h},1};
    uint64_t key=datalab_ui_path_key(path);
    for (uint32_t i=0;i<s->count;++i) if (s->keys[i].domain==domain && s->keys[i].value==key) {
        c.id=s->controls[i].id; break;
    }
    KitUiButtonState state=kit_ui_interaction_button_state(&s->interaction,&c,selected);
    DatalabAuthoringThemePalette p={0};
    p.button_fill_r=palette->list_fill_r; p.button_fill_g=palette->list_fill_g; p.button_fill_b=palette->list_fill_b;
    p.button_active_r=palette->selected_fill_r; p.button_active_g=palette->selected_fill_g; p.button_active_b=palette->selected_fill_b;
    p.button_hover_r=palette->top_fill_r; p.button_hover_g=palette->top_fill_g; p.button_hover_b=palette->top_fill_b;
    p.text_primary_r=palette->text_primary_r; p.text_primary_g=palette->text_primary_g; p.text_primary_b=palette->text_primary_b;
    p.text_secondary_r=palette->frame_r; p.text_secondary_g=palette->frame_g; p.text_secondary_b=palette->frame_b;
    datalab_ui_draw_button(renderer,&r,"",&state,&p);
}
void datalab_picker_apply_button(DatalabPickerActionHost *h, KitUiSurfaceKey key) {
    if (key.domain==DATALAB_PICKER_FILE) {
        for (size_t i=0;i<h->file_count;++i) if (datalab_ui_path_key(h->files[i])==key.value) {
            char root[DATALAB_APP_PATH_CAP];
            if (!datalab_picker_is_regular_file(h->files[i])) snprintf(h->status,h->status_cap,"recent artifact is no longer available");
            else if (!datalab_picker_parent_directory(h->files[i],root,sizeof(root))) snprintf(h->status,h->status_cap,"recent artifact root is unavailable");
            else {
                (void)datalab_input_root_select_recent(h->input_root,h->root_cap,h->roots,h->root_count,DATALAB_RECENT_INPUT_ROOT_LIMIT,root);
                snprintf(h->edit_root,h->root_cap,"%s",h->input_root);
                (void)datalab_picker_refresh_catalog(h->catalog,h->view,h->input_root,DATALAB_INPUT_CATALOG_REFRESH_ROOT_CHANGE,h->status,h->status_cap);
                *h->all_count=datalab_input_catalog_count(h->catalog);
                datalab_picker_view_apply_filter(h->view,h->filter,h->filtered_count);
                if (datalab_input_catalog_file_is_current(h->catalog,h->input_root,core_path_basename(h->files[i]))) {
                    snprintf(h->output,h->output_cap,"%s",h->files[i]); *h->done=1;
                } else snprintf(h->status,h->status_cap,"recent artifact is no longer current in input root");
            }
            return;
        }
    } else if (key.domain==DATALAB_PICKER_ROOT) {
        for (size_t i=0;i<*h->root_count;++i) if (datalab_ui_path_key(h->roots[i])==key.value) {
            char root[DATALAB_APP_PATH_CAP];
            snprintf(root,sizeof(root),"%s",h->roots[i]);
            (void)datalab_input_root_select_recent(h->input_root,h->root_cap,h->roots,h->root_count,DATALAB_RECENT_INPUT_ROOT_LIMIT,root);
            snprintf(h->edit_root,h->root_cap,"%s",h->input_root);
            (void)datalab_picker_refresh_catalog(h->catalog,h->view,h->input_root,DATALAB_INPUT_CATALOG_REFRESH_ROOT_CHANGE,h->status,h->status_cap);
            *h->all_count=datalab_input_catalog_count(h->catalog);
            datalab_picker_view_apply_filter(h->view,h->filter,h->filtered_count);
            *h->selected=0u; datalab_catalog_view_set_selected_index(h->view,0u);
            *h->reveal=1; *h->edit_mode=0; return;
        }
    }
}
