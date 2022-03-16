#ifndef KeyFill_hpp
#define KeyFill_hpp

#include <string>
#include <vector>

#include "Light2D.hpp"

namespace KeyFill {
  class Windows {
    private:
      L2D::Window window;
      L2D::Renderer renderer;
      std::vector<L2D::StreamingTexture> textures;

    public:
      Windows(L2D::L2DInit& l2DInit, std::string title, L2D::Rect rect, int flags)
        : window{l2DInit, title, rect, flags}
        , renderer{window}
        {}

      auto lock(size_t i) {
        while (textures.size() <= i) {
          textures.emplace_back(renderer, L2D::Surface::Format::BGRA32, L2D::Size{1920, 1080});
        }
        return textures[i].lock();
      }

      auto render() {
        renderer.fill
          ( {0, 0, 3840, 1080}
          , {0, 0, 0, 0}
          , L2D::BlendMode::None()
          );
        for (auto& texture : textures) {
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
        }
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
