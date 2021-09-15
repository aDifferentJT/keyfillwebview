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

    public:
      Windows(L2D::L2DInit& l2DInit, std::string title, L2D::Rect rect, int flags)
        : window{l2DInit, title, rect, flags}
        , renderer{window}
        , texture{renderer, L2D::Surface::Format::BGRA32, {1920, 1080}}
        {}

      auto lock() { return texture.lock(); }

      auto render() {
        texture.render
          ( {0, 0, 1920, 1080}
          , L2D::BlendMode::Custom
            //( SDL_BLENDFACTOR_SRC_ALPHA
            ( SDL_BLENDFACTOR_ONE
            , SDL_BLENDFACTOR_ZERO
            , SDL_BLENDOPERATION_ADD
            , SDL_BLENDFACTOR_ONE
            , SDL_BLENDFACTOR_ZERO
            , SDL_BLENDOPERATION_ADD
            )
          );
        renderer.fill({1920, 0, 1920, 1080}, {255, 255, 255, 255});
        texture.render
          ( {1920, 0, 1920, 1080}
          , L2D::BlendMode::Custom
            ( SDL_BLENDFACTOR_ZERO
            , SDL_BLENDFACTOR_SRC_ALPHA
            , SDL_BLENDOPERATION_ADD
            , SDL_BLENDFACTOR_ZERO
            , SDL_BLENDFACTOR_ONE
            , SDL_BLENDOPERATION_ADD
            )
          );
        renderer.present();
      }

      auto render(L2D::Surface& surface) {
        auto texture2 = L2D::Texture{renderer, surface};
        texture2.render
          ( {0, 0, 1920, 1080}
          , L2D::BlendMode::Custom
            //( SDL_BLENDFACTOR_SRC_ALPHA
            ( SDL_BLENDFACTOR_ONE
            , SDL_BLENDFACTOR_ZERO
            , SDL_BLENDOPERATION_ADD
            , SDL_BLENDFACTOR_ONE
            , SDL_BLENDFACTOR_ZERO
            , SDL_BLENDOPERATION_ADD
            )
          );
        renderer.fill({1920, 0, 1920, 1080}, {255, 255, 255, 255});
        texture2.render
          ( {1920, 0, 1920, 1080}
          , L2D::BlendMode::Custom
            ( SDL_BLENDFACTOR_ZERO
            , SDL_BLENDFACTOR_SRC_ALPHA
            , SDL_BLENDOPERATION_ADD
            , SDL_BLENDFACTOR_ZERO
            , SDL_BLENDFACTOR_ONE
            , SDL_BLENDOPERATION_ADD
            )
          );
        renderer.present();
      }
  };
}

#endif
