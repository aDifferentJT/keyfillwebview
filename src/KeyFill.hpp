#ifndef KeyFill_hpp
#define KeyFill_hpp

#include <string>

#include "Light2D.hpp"

namespace KeyFill {
  class Windows {
    private:
      L2D::Window window;
      L2D::Renderer renderer;
      L2D::StreamingTexture texture;
      L2D::StreamingTexture texture2;

    public:
      Windows(L2D::L2DInit& l2DInit, std::string title, L2D::Rect rect, int flags)
        : window{l2DInit, title, rect, flags}
        , renderer{window}
        , texture{renderer, L2D::Surface::Format::BGRA32, {1920, 1080}}
        , texture2{renderer, L2D::Surface::Format::BGRA32, {1920, 1080}}
        {}

      auto lock() { return texture.lock(); }
      auto lock2() { return texture2.lock(); }

      auto render() {
        texture2.render
          ( {0, 0, 1920, 1080}
          , L2D::BlendMode::Custom
            ( SDL_BLENDFACTOR_ONE
            , SDL_BLENDFACTOR_ZERO
            , SDL_BLENDOPERATION_ADD
            , SDL_BLENDFACTOR_ONE
            , SDL_BLENDFACTOR_ZERO
            , SDL_BLENDOPERATION_ADD
            )
          );
        texture.render
          ( {0, 0, 1920, 1080}
          , L2D::BlendMode::Custom
            ( SDL_BLENDFACTOR_ONE
            , SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
            , SDL_BLENDOPERATION_ADD
            , SDL_BLENDFACTOR_ONE
            , SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
            , SDL_BLENDOPERATION_ADD
            )
          );
        texture2.render
          ( {1920, 0, 1920, 1080}
          , L2D::BlendMode::Custom
            ( SDL_BLENDFACTOR_ONE
            , SDL_BLENDFACTOR_ZERO
            , SDL_BLENDOPERATION_ADD
            , SDL_BLENDFACTOR_ONE
            , SDL_BLENDFACTOR_ZERO
            , SDL_BLENDOPERATION_ADD
            )
          );
        texture.render
          ( {1920, 0, 1920, 1080}
          , L2D::BlendMode::Custom
            ( SDL_BLENDFACTOR_ONE
            , SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
            , SDL_BLENDOPERATION_ADD
            , SDL_BLENDFACTOR_ONE
            , SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
            , SDL_BLENDOPERATION_ADD
            )
          );
        renderer.fill
          ( {1920, 0, 1920, 1080}
          , {255, 255, 255, 255}
          , L2D::BlendMode::Custom
            ( SDL_BLENDFACTOR_DST_ALPHA
            , SDL_BLENDFACTOR_ZERO
            , SDL_BLENDOPERATION_ADD
            , SDL_BLENDFACTOR_ONE
            , SDL_BLENDFACTOR_ZERO
            , SDL_BLENDOPERATION_ADD
            )
          );
        renderer.present();
      }

      auto renderClear() {
        renderer.fill({0, 0, 3840, 1080}, {0, 0, 0, 0});
        renderer.present();
      }

      auto renderBlack() {
        renderer.fill({0, 0, 1920, 1080}, {0, 0, 0, 0});
        renderer.fill({1920, 0, 1920, 1080}, {255, 255, 255, 255});
        renderer.present();
      }
  };
}

#endif
