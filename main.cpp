#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <SDL3_image/SDL_image.h>
#include <cstdint>
#include <vector>

unsigned int windowWidth = 480;
unsigned int windowHeight = 360;
const unsigned int padding = 16;
const uint8_t alphaThreshold = 0;

unsigned int resolution = 16; // TODO: make changable

std::vector<SDL_Rect> generateCollisionRects(SDL_Surface *surface) {
  std::vector<SDL_Rect> collisionRects;
  if (!surface)
    return collisionRects;

  SDL_LockSurface(surface);

  auto isSolid = [&](int x, int y) {
    if (x < 0 || x >= surface->w || y < 0 || y >= surface->h)
      return false;
    uint32_t pixel = ((uint32_t *)surface->pixels)[y * surface->w + x];
    uint8_t r, g, b, a;
    SDL_GetRGBA(pixel, SDL_GetPixelFormatDetails(surface->format),
                SDL_GetSurfacePalette(surface), &r, &g, &b, &a);
    return a > alphaThreshold;
  };

  std::vector<std::vector<bool>> visited(surface->h,
                                         std::vector<bool>(surface->w, false));

  while (true) {
    SDL_Rect bestRect = {0, 0, 0, 0};
    int bestRectArea = 0;

    for (int y = 0; y < surface->h; ++y) {
      for (int x = 0; x < surface->w; ++x) {
        if (visited[y][x] || !isSolid(x, y))
          continue;

        int w = 1;
        while (x + w < surface->w && !visited[y][x + w] && isSolid(x + w, y))
          w++;

        int h = 1;
        while (y + h < surface->h) {
          bool canExpand = true;
          for (int i = 0; i < w; ++i) {
            if (visited[y + h][x + i] || !isSolid(x + i, y + h)) {
              canExpand = false;
              break;
            }
          }
          if (canExpand) {
            h++;
            continue;
          }
          break;
        }

        if (w * h > bestRectArea) {
          bestRect = {x, y, w, h};
          bestRectArea = w * h;
        }
      }
    }

    if (bestRectArea == 0)
      break;

    collisionRects.push_back(bestRect);

    for (int cy = 0; cy < bestRect.h; ++cy)
      for (int cx = 0; cx < bestRect.w; ++cx)
        if (bestRect.y + cy < surface->h && bestRect.x + cx < surface->w)
          visited[bestRect.y + cy][bestRect.x + cx] = true;
  }

  SDL_UnlockSurface(surface);
  return collisionRects;
}

// TODO: Add ability to load custom file.

int main() {
  SDL_Window *window;
  SDL_Renderer *renderer;

  // TODO: error handling
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  SDL_CreateWindowAndRenderer("Scratch Everywhere! Collision Test", windowWidth,
                              windowHeight, SDL_WINDOW_RESIZABLE, &window,
                              &renderer);

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  SDL_Texture *sourceImage = IMG_LoadTexture(renderer, "source.png");
  float width, height;
  SDL_GetTextureSize(sourceImage, &width, &height);

  SDL_FRect sourceDest = {padding, padding,
                          (windowHeight - padding * 2) * (width / height),
                          ((float)windowHeight - padding * 2)};

  SDL_Surface *sourceSurface = IMG_Load("source.png");

  SDL_Surface *scaledSurface = SDL_CreateSurface(
      (width > height) ? resolution : resolution * ((double)width / height),
      (height > width) ? resolution : resolution * ((double)height / width),
      SDL_PIXELFORMAT_RGBA8888);

  SDL_BlitSurfaceScaled(sourceSurface, NULL, scaledSurface, NULL,
                        SDL_SCALEMODE_NEAREST);
  std::vector<SDL_Rect> collisionRects = generateCollisionRects(scaledSurface);

  unsigned char white =
      SDL_GetSystemTheme() == SDL_SYSTEM_THEME_DARK ? 0x00 : 0xff;
  bool overlap = false;

  SDL_Event event;
  while (1) {
    SDL_PollEvent(&event);
    if (event.type == SDL_EVENT_QUIT) {
      break;
    }
    if (event.type == SDL_EVENT_KEY_DOWN) {
      switch (event.key.scancode) {
      case SDL_SCANCODE_SPACE:
        white = white == 0x00 ? 0xff : 0x00;
        break;
      case SDL_SCANCODE_O:
        overlap = !overlap;
        break;
      default:
        break;
      }
    }
    if (event.type == SDL_EVENT_WINDOW_RESIZED) {
      SDL_GetWindowSizeInPixels(window, (int *)&windowWidth,
                                (int *)&windowHeight);
      sourceDest = {padding, padding,
                    (windowHeight - padding * 2) * (width / height),
                    ((float)windowHeight - padding * 2)};
    }

    SDL_SetRenderDrawColor(renderer, white, white, white, 0xff);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, sourceImage, nullptr, &sourceDest);

    SDL_FRect collisionDest = sourceDest;
    if (!overlap)
      collisionDest.x = windowWidth - collisionDest.w - padding;

    for (const auto &rect : collisionRects) {
      SDL_FRect drawRect = {
          collisionDest.x + rect.x * (collisionDest.w / scaledSurface->w),
          collisionDest.y + rect.y * (collisionDest.h / scaledSurface->h),
          rect.w * (collisionDest.w / scaledSurface->w),
          rect.h * (collisionDest.h / scaledSurface->h)};
      SDL_SetRenderDrawColor(renderer, 255, 0, 0, overlap ? 128 : 64);
      SDL_RenderFillRect(renderer, &drawRect);
      SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
      SDL_RenderRect(renderer, &drawRect);
    }

    SDL_RenderPresent(renderer);
  }

  SDL_DestroySurface(sourceSurface);
  SDL_DestroySurface(scaledSurface);
  SDL_DestroyTexture(sourceImage);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);

  SDL_Quit();

  return 0;
}
