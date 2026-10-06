#include "datalab_sdl_capture.h"
int datalab_sdl_capture(SDL_Renderer *renderer,const char *path) {
    int w=0,h=0;
    if(!renderer || !path || !*path || SDL_GetRendererOutputSize(renderer,&w,&h)!=0 || w<=0 || h<=0)return 0;
    SDL_Surface *pixels=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32);
    if(!pixels)return 0;
    int ok=SDL_RenderReadPixels(renderer,NULL,pixels->format->format,pixels->pixels,pixels->pitch)==0 && SDL_SaveBMP(pixels,path)==0;
    SDL_FreeSurface(pixels);return ok;
}
