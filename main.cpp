#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <cstdint>
#include <string>
#include <vector>

unsigned int windowWidth = 480;
unsigned int windowHeight = 360;
const unsigned int padding = 16;
const uint8_t alphaThreshold = 0;

const unsigned int minResolution = 6;
const unsigned int maxResolution = 80;

unsigned int resolution = 16;

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

SDL_Window *window;
SDL_Renderer *renderer;

bool loaded = false;

SDL_Surface *sourceSurface;
SDL_Surface *scaledSurface;
SDL_Texture *sourceImage;
SDL_FRect sourceDest;
std::vector<SDL_Rect> collisionRects;
float width, height;

const SDL_DialogFileFilter filters[] = {{"PNG images", "png"},
                                        {"JPEG images", "jpg;jpeg"},
                                        {"All images", "png;jpg;jpeg"},
                                        {"All files", "*"}};

int main() {
  // TODO: error handling
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  SDL_CreateWindowAndRenderer("Scratch Everywhere! Collision Test", windowWidth,
                              windowHeight, SDL_WINDOW_RESIZABLE, &window,
                              &renderer);

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  auto onResolutionChange = [&]() {
    SDL_DestroySurface(scaledSurface);
    scaledSurface = SDL_CreateSurface(
        (width > height) ? resolution : resolution * ((double)width / height),
        (height > width) ? resolution : resolution * ((double)height / width),
        SDL_PIXELFORMAT_RGBA8888);

    SDL_BlitSurfaceScaled(sourceSurface, NULL, scaledSurface, NULL,
                          SDL_SCALEMODE_NEAREST);
    collisionRects = generateCollisionRects(scaledSurface);
  };

  SDL_ShowOpenFileDialog(
      [](void *userdata, const char *const *filelist, int filter) {
        if (!filelist) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "An error occured: %s",
                       SDL_GetError());
          return;
        }
        if (!*filelist) {
          SDL_Log("The user did not select any file.");
          return;
        }

        sourceImage = IMG_LoadTexture(renderer, *filelist);
        SDL_GetTextureSize(sourceImage, &width, &height);

        sourceDest = {padding, padding,
                      (windowHeight - padding * 2) * (width / height),
                      ((float)windowHeight - padding * 2)};

        sourceSurface = IMG_Load(*filelist);

        scaledSurface = SDL_CreateSurface(
            (width > height) ? resolution
                             : resolution * ((double)width / height),
            (height > width) ? resolution
                             : resolution * ((double)height / width),
            SDL_PIXELFORMAT_RGBA8888);

        SDL_BlitSurfaceScaled(sourceSurface, NULL, scaledSurface, NULL,
                              SDL_SCALEMODE_NEAREST);
        collisionRects = generateCollisionRects(scaledSurface);

        loaded = true;
      },
      NULL, window, filters, 4, NULL, false);

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
        if (!loaded)
          break;
        overlap = !overlap;
        break;
      case SDL_SCANCODE_UP:
        if (resolution == maxResolution || !loaded)
          break;
        resolution++;
        onResolutionChange();
        break;
      case SDL_SCANCODE_DOWN:
        if (resolution == minResolution || !loaded)
          break;
        resolution--;
        onResolutionChange();
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

    if (loaded) {
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

      SDL_SetRenderDrawColor(renderer, white == 0x00 ? 0xff : 0x00,
                             white == 0x00 ? 0xff : 0x00,
                             white == 0x00 ? 0xff : 0x00, 255);
      SDL_RenderDebugText(
          renderer,
          windowWidth / 2.0f - 48 - std::to_string(resolution).length() * 4,
          padding, ("Resolution: " + std::to_string(resolution)).c_str());
    } else {
      SDL_SetRenderDrawColor(renderer, white == 0x00 ? 0xff : 0x00,
                             white == 0x00 ? 0xff : 0x00,
                             white == 0x00 ? 0xff : 0x00, 255);
      SDL_RenderDebugText(renderer, windowWidth / 2.0f - 80,
                          windowHeight / 2.0f - 4, "Waiting for image...");
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
