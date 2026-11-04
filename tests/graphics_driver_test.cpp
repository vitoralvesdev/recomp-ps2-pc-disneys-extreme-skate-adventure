#include "raylib.h"
#include "rlgl.h"
#include <cstdio>
#include <cstdlib>
extern "C" __declspec(dllimport) unsigned int __stdcall glGetError(void);
int main(){
 SetTraceLogLevel(LOG_INFO);
 SetConfigFlags(FLAG_WINDOW_HIDDEN);
 InitWindow(128,128,"Disney port - isolated GPU test");
 if(!IsWindowReady()){std::fprintf(stderr,"FAIL: OpenGL window initialization\n");return 1;}
 Image source=GenImageColor(64,64,BLACK);
 for(int y=0;y<64;++y) for(int x=0;x<64;++x) ImageDrawPixel(&source,x,y,Color{static_cast<unsigned char>(x*4),static_cast<unsigned char>(y*4),static_cast<unsigned char>((x^y)*4),255});
 Texture2D texture=LoadTextureFromImage(source);
 SetTextureFilter(texture,TEXTURE_FILTER_POINT);
 RenderTexture2D target=LoadRenderTexture(64,64);
 BeginTextureMode(target);ClearBackground(BLACK);DrawTexture(texture,0,0,WHITE);EndTextureMode();
 Image result=LoadImageFromTexture(target.texture);
 unsigned direct=0,flipped=0;
 for(int y=0;y<64;++y) for(int x=0;x<64;++x){
  Color got=GetImageColor(result,x,y),expected=GetImageColor(source,x,y),upside=GetImageColor(source,x,63-y);
  auto same=[](Color a,Color b){return a.r==b.r&&a.g==b.g&&a.b==b.b;};
  direct+=!same(got,expected);flipped+=!same(got,upside);
 }


 const unsigned error=glGetError();
 const bool passed=(direct==0||flipped==0)&&error==0;
 std::printf("%s: OpenGL shader, texture upload, framebuffer rendering and readback; directDiff=%u flippedDiff=%u GLerror=0x%x\n",passed?"PASS":"FAIL",direct,flipped,error);
 ExportImage(result,"boot-driver-test.png");
 UnloadImage(result);UnloadRenderTexture(target);UnloadTexture(texture);UnloadImage(source);CloseWindow();
 return passed?0:1;
}

