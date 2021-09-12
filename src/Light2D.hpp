#ifndef Light2D_hpp
#define Light2D_hpp

#include <cstdio>
#include <iostream>

#include <cmath>
#include <cstring>
#include <concepts>
#include <functional>
#include <iterator>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include "sdl.hpp"

namespace L2D {

  class L2DWitness {
    private:
      L2DWitness() = default;

      friend class L2DInit;
  };

  class L2DInit : public L2DWitness {
    public:
      L2DInit() {
        if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
          std::cerr << "SDL Init failed\n";
          std::terminate();
        }
      }
      L2DInit(const L2DInit&) = delete;
      L2DInit(L2DInit&&) = delete;
      L2DInit& operator=(const L2DInit&) = delete;
      L2DInit& operator=(L2DInit&&) = delete;
      ~L2DInit() { SDL_Quit(); }
  };

  struct Point : public SDL_Point {
    Point(int x, int y) : SDL_Point{x, y} {}

    auto xPos() { return x; }
    auto yPos() { return y; }

    auto operator-() { return Point{-x, -y}; }

    friend auto operator+(Point lhs, Point rhs) { return Point{lhs.x + rhs.x, lhs.y + rhs.y}; }
    friend auto operator-(Point lhs, Point rhs) { return Point{lhs.x - rhs.x, lhs.y - rhs.y}; }
  };

  struct Size {
    int w;
    int h;

    friend auto operator*(Size lhs, auto rhs) {
      return Size
        { static_cast<int>(lhs.w * rhs)
        , static_cast<int>(lhs.h * rhs)
        };
    }
    friend auto operator/(Size lhs, auto rhs) {
      return Size
        { static_cast<int>(lhs.w / rhs)
        , static_cast<int>(lhs.h / rhs)
        };
    }
  };

  struct Rect : public SDL_Rect {
    Rect(int x, int y, int w, int h) : SDL_Rect{x, y, w, h} {}
    Rect(Point p, Size s) : Rect{p.x, p.y, s.w, s.h} {}

    auto width()  const { return w; }
    auto height() const { return h; }

    auto top()    const { return y; }
    auto bottom() const { return y + h; }
    auto left()   const { return x; }
    auto right()  const { return x + w; }

    auto point() const { return Point{x, y}; }
    auto size()  const { return Size{w, h}; }

    auto center() const { return Point{x + w / 2, y + h / 2}; }
    static auto centeredOn(Point p, Size s) {
      return Rect{p - Point{s.w / 2, s.h / 2}, s};
    }

    friend auto operator==(Rect lhs, Rect rhs) {
      return
        (  lhs.x == rhs.x
        && lhs.y == rhs.y
        && lhs.w == rhs.w
        && lhs.h == rhs.h
        );
    }

    friend auto operator+(Rect lhs, Point rhs) { return Rect{lhs.point() + rhs, lhs.size()}; }
    friend auto operator-(Rect lhs, Point rhs) { return Rect{lhs.point() - rhs, lhs.size()}; }

    friend auto operator|(Rect lhs, Rect rhs) {
      auto top    = std::min(lhs.top(),    rhs.top());
      auto bottom = std::max(lhs.bottom(), rhs.bottom());
      auto left   = std::min(lhs.left(),   rhs.left());
      auto right  = std::max(lhs.right(),  rhs.right());
      return Rect{left, top, right - left, bottom - top};
    }

    friend auto lerp(Rect a, Rect b, auto t) {
      return Rect
        { static_cast<int>(std::lerp(a.x, b.x, t))
        , static_cast<int>(std::lerp(a.y, b.y, t))
        , static_cast<int>(std::lerp(a.w, b.w, t))
        , static_cast<int>(std::lerp(a.h, b.h, t))
        };
    }
  };

  struct Colour {
    public:
      uint8_t r;
      uint8_t g;
      uint8_t b;
      uint8_t a = 0xFF;

      auto withAlpha(int alpha) const { return Colour{r, g, b, static_cast<uint8_t>(a * alpha / 255)}; }

      friend auto lerp(Colour a, Colour b, auto t) {
        constexpr auto square = [](auto x) { return x * x; };
        return Colour
          { static_cast<uint8_t>(std::sqrt(std::lerp(square(a.r), square(b.r), t)))
          , static_cast<uint8_t>(std::sqrt(std::lerp(square(a.g), square(b.g), t)))
          , static_cast<uint8_t>(std::sqrt(std::lerp(square(a.b), square(b.b), t)))
          , static_cast<uint8_t>(std::sqrt(std::lerp(square(a.a), square(b.a), t)))
          };
      }

    private:
      auto mapToFormat(const SDL_PixelFormat* format) {
        return SDL_MapRGBA(format, r, g, b, a);
      }

      operator SDL_Colour() {
        return SDL_Colour{r, g, b, a};
      }

      friend class Surface;
  };

  class BlendMode {
    private:
      SDL_BlendMode blendMode;

      BlendMode(SDL_BlendMode blendMode) : blendMode{blendMode} {}

    public:
      static auto None()  { return BlendMode{SDL_BLENDMODE_NONE}; }
      static auto Blend() { return BlendMode{SDL_BLENDMODE_BLEND}; }
      static auto Add()   { return BlendMode{SDL_BLENDMODE_ADD}; }
      static auto Mod()   { return BlendMode{SDL_BLENDMODE_MOD}; }
      static auto Custom
        ( SDL_BlendFactor    srcColorFactor
        , SDL_BlendFactor    dstColorFactor
        , SDL_BlendOperation colorOperation
        , SDL_BlendFactor    srcAlphaFactor
        , SDL_BlendFactor    dstAlphaFactor
        , SDL_BlendOperation alphaOperation
        ) {
        return BlendMode
          { SDL_ComposeCustomBlendMode
            ( srcColorFactor
            , dstColorFactor
            , colorOperation
            , srcAlphaFactor
            , dstAlphaFactor
            , alphaOperation
            )
          };
      }

      friend class Surface;
      friend class Texture;
      friend class StreamingTexture;
  };

  // Should be a lambda but that doesn't work, something about not exporting operator=
  template <auto f>
  struct lambdaFor {
    template <typename ...Args>
    auto operator()(Args ...args) {
      f(std::forward<Args>(args)...);
    }
  };

  class Surface : public L2DWitness {
    private:
      std::unique_ptr<SDL_Surface, lambdaFor<SDL_FreeSurface>> surface; 

      Surface(L2DWitness l2DWitness, SDL_Surface* rawSurface) : L2DWitness{l2DWitness}, surface{rawSurface} {}

    public:
      Surface() = delete;

      enum class Format
        { RGBA32 = SDL_PIXELFORMAT_RGBA32
        , BGRA32 = SDL_PIXELFORMAT_BGRA32
        };

      Surface(L2DWitness l2DWitness, Size size, Format format = Format::RGBA32)
        : Surface{l2DWitness, SDL_CreateRGBSurfaceWithFormat(0, size.w, size.h, 0, static_cast<SDL_PixelFormatEnum>(format))} {}

      auto width()  const { return surface->w; }
      auto height() const { return surface->h; }
      auto rect() const { return Rect{0, 0, surface->w, surface->h}; }

      void fill(Colour colour) {
        SDL_FillRect(surface.get(), nullptr, colour.mapToFormat(surface->format));
      }
      void fill(Rect rect, Colour colour) {
        SDL_FillRect(surface.get(), &rect, colour.mapToFormat(surface->format));
      }

      void blit(Surface const & src, Point dstTopLeft = {0, 0}, Uint8 alpha = 255, BlendMode blendMode = BlendMode::Blend()) {
        SDL_SetSurfaceAlphaMod(src.surface.get(), alpha);
        SDL_SetSurfaceBlendMode(src.surface.get(), blendMode.blendMode);
        auto dstRect = Rect{dstTopLeft.x, dstTopLeft.y, 0, 0};
        SDL_BlitSurface(src.surface.get(), nullptr, this->surface.get(), &dstRect);
      }

      void blit(Surface const & src, Rect srcRect, Point dstTopLeft, Uint8 alpha = 255, BlendMode blendMode = BlendMode::Blend()) {
        SDL_SetSurfaceAlphaMod(src.surface.get(), alpha);
        SDL_SetSurfaceBlendMode(src.surface.get(), blendMode.blendMode);
        auto dstRect = Rect{dstTopLeft.x, dstTopLeft.y, 0, 0};
        SDL_BlitSurface(src.surface.get(), &srcRect, this->surface.get(), &dstRect);
      }

      void blit(Surface const & src, Rect dstRect, Uint8 alpha = 255, BlendMode blendMode = BlendMode::Blend()) {
        SDL_SetSurfaceAlphaMod(src.surface.get(), alpha);
        SDL_SetSurfaceBlendMode(src.surface.get(), blendMode.blendMode);
        SDL_BlitScaled(src.surface.get(), nullptr, this->surface.get(), &dstRect);
      }

      void blit(Surface const & src, Rect srcRect, Rect dstRect, Uint8 alpha = 255, BlendMode blendMode = BlendMode::Blend()) {
        SDL_SetSurfaceAlphaMod(src.surface.get(), alpha);
        SDL_SetSurfaceBlendMode(src.surface.get(), blendMode.blendMode);
        SDL_BlitScaled(src.surface.get(), &srcRect, this->surface.get(), &dstRect);
      }

      // Assume that the dimensions match
      void unsafe_blit_buffer(void const * buffer) {
        std::memcpy(surface->pixels, buffer, surface->pitch * surface->h);
      }

      friend class Texture;
  };

  class Window {
    private:
      SDL_Window* window; 

    public:
      Window(L2DWitness, std::string title, Rect rect, int flags)
        : window{SDL_CreateWindow(title.c_str(), rect.x, rect.y, rect.w, rect.h, flags)}
      {
        if (window == nullptr) {
          std::cerr << "Could not create window\n";
          std::terminate();
        }
      }

      Window() = delete;

      Window(const Window&) = delete;
      Window& operator=(const Window&) = delete;

      Window(Window&& that) : window{that.window} {
        that.window = nullptr;
      };

      friend void swap(Window& lhs, Window& rhs) {
        std::swap(lhs.window, rhs.window);
      }

      Window& operator=(Window&& that) {
        auto tmp = Window{std::move(that)};
        swap(*this, tmp);
        return *this;
      }

      ~Window() { SDL_DestroyWindow(window); }

      friend class Renderer;
  };

  class Renderer {
    private:
      std::unique_ptr<SDL_Renderer, lambdaFor<SDL_DestroyRenderer>> renderer; 

    public:
      Renderer() = delete;

      Renderer(Window& window)
        : renderer{SDL_CreateRenderer(window.window, -1, 0)}
        //: renderer{SDL_CreateRenderer(window.window, -1, SDL_RENDERER_PRESENTVSYNC)}
        {
        if (renderer == nullptr) {
          std::cerr << "Could not create renderer\n";
          std::terminate();
        }
      }

      void fill(Colour colour) {
        SDL_SetRenderDrawColor
          ( renderer.get()
          , colour.r
          , colour.g
          , colour.b
          , colour.a
          );
        SDL_RenderClear(renderer.get());
      }

      void fill(Rect rect, Colour colour) {
        SDL_SetRenderDrawColor
          ( renderer.get()
          , colour.r
          , colour.g
          , colour.b
          , colour.a
          );
        SDL_RenderFillRect(renderer.get(), &rect);
      }

      void present() { SDL_RenderPresent(renderer.get()); }

      friend class Texture;
      friend class StreamingTexture;
  };

  class Texture {
    private:
      SDL_Renderer* rawRenderer;
      std::unique_ptr<SDL_Texture, void(*)(SDL_Texture* t)> texture;

    public:
      Texture() = delete;

      Texture(Renderer& renderer, Surface& surface)
        : rawRenderer{renderer.renderer.get()}
        , texture{SDL_CreateTextureFromSurface(rawRenderer, surface.surface.get()), SDL_DestroyTexture}
        {}

      void render(BlendMode blendMode) {
        if (0 != SDL_SetTextureBlendMode(texture.get(), blendMode.blendMode)) {
          std::cerr << SDL_GetError();
        }
        SDL_RenderCopy(rawRenderer, texture.get(), nullptr, nullptr);
      }

      void render(Rect dst, BlendMode blendMode) {
        if (0 != SDL_SetTextureBlendMode(texture.get(), blendMode.blendMode)) {
          std::cerr << SDL_GetError();
        }
        SDL_RenderCopy(rawRenderer, texture.get(), nullptr, &dst);
      }
  };

  class StreamingTexture {
    private:
      SDL_Renderer* rawRenderer;
      Size size;
      std::unique_ptr<SDL_Texture, void(*)(SDL_Texture* t)> texture;

    public:
      StreamingTexture() = delete;

      StreamingTexture(Renderer& renderer, Surface::Format format, Size size)
        : rawRenderer{renderer.renderer.get()}
        , size{size}
        , texture{SDL_CreateTexture(rawRenderer, static_cast<SDL_PixelFormatEnum>(format), SDL_TEXTUREACCESS_STREAMING, size.w, size.h), SDL_DestroyTexture}
        {}

      void render(BlendMode blendMode) {
        if (0 != SDL_SetTextureBlendMode(texture.get(), blendMode.blendMode)) {
          std::cerr << SDL_GetError();
        }
        SDL_RenderCopy(rawRenderer, texture.get(), nullptr, nullptr);
      }

      void render(Rect dst, BlendMode blendMode) {
        if (0 != SDL_SetTextureBlendMode(texture.get(), blendMode.blendMode)) {
          std::cerr << SDL_GetError();
        }
        SDL_RenderCopy(rawRenderer, texture.get(), nullptr, &dst);
      }

      class Unlocker {
        private:
          SDL_Texture* texture;

          Unlocker(SDL_Texture* texture) : texture{texture} {}

        public:
          void operator()(void* p) const {
            if (p) {
              SDL_UnlockTexture(texture);
            }
          }

          friend class StreamingTexture;
      };

      struct Buffer {
        std::unique_ptr<void, Unlocker> pixels;
        int pitch;
      };

      auto lock() -> Buffer {
        void* buffer;
        int pitch;
        if (0 != SDL_LockTexture(texture.get(), nullptr, &buffer, &pitch)) {
          std::cerr << SDL_GetError();
        }
        return {{buffer, Unlocker{texture.get()}}, pitch};
      }
  };

  void delay(uint32_t milliseconds) {
    SDL_Delay(milliseconds);
  }
};

#endif
