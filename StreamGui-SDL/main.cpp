#include <iostream>

#include "../ParserStream/StreamApp.h"

#define SDL_MAIN_HANDLED
#include "SDL.h"

bool inited = false;
int imgW = -1;
int imgH = -1;

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;
SDL_Rect textureRect;

int quit = 0;

void Init(const int w, const int h)
{
    imgW = w;
    imgH = h;

    if(!inited) {

        window = SDL_CreateWindow("StreamGui", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                  imgW, imgH, SDL_WINDOW_RESIZABLE );
        if(!window) {
            SDL_LogError(1, "SDL: SDL_CreateWindow failed.");
            exit(1);
        }
        
        inited = true;
    }
    else {
        SDL_SetWindowSize(window, imgW, imgH);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
    }

    SDL_SetWindowMinimumSize(window, 320, 320.f*imgH/imgW);

    renderer = SDL_CreateRenderer(window, -1, 0);
    if(!renderer) {
        SDL_LogError(1, "SDL: SDL_CreateRenderer failed.");
        exit(1);
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR24,
                                SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, imgW, imgH);
    if(!texture) {
        SDL_LogError(1, "SDL: SDL_CreateTexture failed.");
        exit(1);
    }

    textureRect = SDL_Rect{0, 0, imgW, imgH};    
}

inline void Update(const RGBImage& img)
{
    SDL_UpdateTexture(texture, &textureRect, img.rawData.data(), img.width * 3);

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);  
    SDL_RenderPresent(renderer);   
}

int StreamCallback(void *data)
{

    StreamApp* app = (StreamApp*)data;

    bool ret =  app->StartImageStream([](RGBImage img) {
        if(img.width != imgW || img.height != imgH) {
            Init(img.width, img.height);
        }
        Update(img);
        SDL_Delay(30);
    });

    if(!ret) {
        SDL_Event eve; 
        eve.type = SDL_QUIT;
        SDL_PushEvent(&eve);
    }

    while(!quit) {
        SDL_Delay(1000);
    }

    return 0;
}

int main(int argc, char* argv[])
{
    char *ip = nullptr;
    int port = 8000;
    bool tcp = false;

    if(argc == 2) { ip = argv[1];    }
    else if(argc == 3) { ip = argv[1]; port = atoi(argv[2]);  }
    else if(argc == 4) { ip = argv[1]; port = atoi(argv[2]);  tcp = true;  }

    if (!ip){
        ip = (char*)malloc(15);
        strcpy_s(ip,15, "0.0.0.0");
    }
    printf("Set ip: %s\n", ip);
    printf("Set port: %d\n", port);
    printf("Set tcp: %s\n\n", tcp? "true":"false");



    if(SDL_Init(SDL_INIT_VIDEO)){
        SDL_Log("init audio subsysytem failed.");
        return 0;
    }

    Init(640,480); 

    StreamApp app(ip, port, tcp);

    SDL_CreateThread(StreamCallback, nullptr, &app);


    SDL_Event event;
    while(1) {
        SDL_WaitEvent(&event);
        if(event.type == SDL_QUIT) {
            quit = 1;
            break;
        }
    }


    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}