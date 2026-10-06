#include "render/render_view_picker_text_presentation.h"
#include <math.h>
#include "kit_ui_native_text_sdl.h"
static CoreResult measure(void *unused,const char *text,float *width) {
    (void)unused;int w=0,h=0;
    if(text[0])datalab_measure_text(1,text,&w,&h);
    *width=(float)w;return core_result_ok();
}
typedef struct TextPaint {SDL_Renderer *renderer;SDL_Color color;} TextPaint;
static CoreResult paint(void *user,KitRenderVec2 at,const char *text) {
    TextPaint *p=user;draw_text_5x7(p->renderer,(int)lroundf(at.x),(int)lroundf(at.y),text,1,p->color.r,p->color.g,p->color.b,p->color.a);
    return core_result_ok();
}
CoreResult datalab_picker_text_present(SDL_Renderer *renderer,KitUiTextEdit *edit,KitUiTextPresentation *view,
    SDL_Rect rect,int active,SDL_Color text,SDL_Color accent) {
    KitUiTextPresentationOptions options={{rect.x,rect.y,rect.w,rect.h},datalab_text_line_height(1),datalab_scaled_px(1),0,0,active,1};
    CoreResult r=kit_ui_text_presentation_build(view,edit,&options,measure,NULL);if(r.code!=CORE_OK)return r;
    int width=0,height=0;float sx=1,sy=1;datalab_renderer_backend_output_size(renderer,&width,&height);SDL_RenderGetScale(renderer,&sx,&sy);
    r=kit_ui_native_text_rect_sdl(NULL,view,width/sx,height/sy,NULL);if(r.code!=CORE_OK)return r;
    KitRenderColor c={accent.r,accent.g,accent.b,accent.a};KitUiTextPresentationColors colors={c,c,c};colors.selection.a=72;
    TextPaint user={renderer,text};return kit_ui_text_presentation_sdl(renderer,view,&colors,paint,&user);
}
CoreResult datalab_picker_text_hit(KitUiTextEdit *edit,const KitUiTextPresentation *view,float x,float y) {
    size_t position;CoreResult r=kit_ui_text_presentation_hit(view,x,y,measure,NULL,&position);
    return r.code==CORE_OK?kit_ui_text_position(edit,position,position):r;
}
