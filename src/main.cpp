
#include "include/base/cef_logging.h"
#include "include/cef_app.h"
#include "include/cef_command_line.h"

#include "KeyFill.hpp"
#include "Light2D.hpp"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

class Client : public CefClient, public CefRenderHandler {
  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(Client);

  private:
    L2D::L2DWitness l2DWitness;
    KeyFill::Windows& keyFill;
    std::mutex& keyFillLockedSemaphore;
    std::mutex& keyFillUnlockedSemaphore;
    std::optional<L2D::StreamingTexture::Buffer>& keyFillLocked;

  public:
    Client
      ( L2D::L2DWitness l2DWitness
      , KeyFill::Windows& keyFill
      , std::mutex& keyFillLockedSemaphore
      , std::mutex& keyFillUnlockedSemaphore
      , std::optional<L2D::StreamingTexture::Buffer>& keyFillLocked
      )
      : l2DWitness{l2DWitness}
      , keyFill{keyFill}
      , keyFillLockedSemaphore{keyFillLockedSemaphore}
      , keyFillUnlockedSemaphore{keyFillUnlockedSemaphore}
      , keyFillLocked{keyFillLocked}
      {}

    // CefClient methods
    auto GetRenderHandler() -> CefRefPtr<CefRenderHandler> override { return this; }
  
    // CefRenderHandler methods
    auto GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo& screen_info) -> bool override {
      screen_info = {1.0, 32, 8, false, {0, 0, 1920, 1080}, {0, 0, 1920, 1080}};
      return true;
    }
  
    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
      rect = {0, 0, 1920, 1080};
    }
  
    void OnPaint
      ( CefRefPtr<CefBrowser> browser
      , PaintElementType type
      , RectList const & dirtyRects
      , void const * buffer
      , int width
      , int height
      ) override {
/*
      auto surface = L2D::Surface{l2DWitness, {1920, 1080}, L2D::Surface::Format::BGRA32};
      surface.unsafe_blit_buffer(buffer);
      keyFill.render(surface);

      {
        auto dst = keyFill.lock();
        std::memcpy(dst.pixels.get(), buffer, dst.pitch * 1080);
      }
      keyFill.render();
*/

      keyFillLockedSemaphore.lock();
      std::memcpy(keyFillLocked->pixels.get(), buffer, keyFillLocked->pitch * 1080);
      keyFillUnlockedSemaphore.unlock();
    }
};

class App : public CefApp, public CefBrowserProcessHandler {
  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(App);

  public:
    L2D::L2DWitness* l2DWitness;
    KeyFill::Windows* keyFill;
    std::mutex* keyFillLockedSemaphore;
    std::mutex* keyFillUnlockedSemaphore;
    std::optional<L2D::StreamingTexture::Buffer>* keyFillLocked;

    // CefApp methods
    auto GetBrowserProcessHandler() -> CefRefPtr<CefBrowserProcessHandler> override { return this; }
  
    // CefBrowserProcessHandler methods
    void OnContextInitialized() override {
      auto info = CefWindowInfo{};
  
      info.SetAsWindowless(0);
      
      auto settings = CefBrowserSettings{};

      settings.windowless_frame_rate = 25;

      auto client = CefRefPtr<Client>
        { new Client
          { *l2DWitness
          , *keyFill
          , *keyFillLockedSemaphore
          , *keyFillUnlockedSemaphore
          , *keyFillLocked
          }
        };

      CefBrowserHost::CreateBrowser(info, client, "http://192.168.254.140:9090/graphics", settings, nullptr, nullptr);
    }
};

auto main(int argc, char** argv) -> int {
  auto mainArgs = CefMainArgs{argc, argv};

  auto app = CefRefPtr<App>{new App{}};

  if (auto exitCode = CefExecuteProcess(mainArgs, app, nullptr); exitCode >= 0) {
    return exitCode;
  }

  auto l2DInit = L2D::L2DInit{};
  auto keyFill = KeyFill::Windows
    { l2DInit
    , "Web View"
    , {0, 0, 3840, 1080}
    , SDL_WINDOW_BORDERLESS
    };

  auto keyFillLockedSemaphore = std::mutex{};
  auto keyFillUnlockedSemaphore = std::mutex{};
  auto keyFillLocked = std::optional{keyFill.lock()};

  keyFillLockedSemaphore.lock();

  app->l2DWitness = &l2DInit;
  app->keyFill = &keyFill;
  app->keyFillLockedSemaphore = &keyFillLockedSemaphore;
  app->keyFillUnlockedSemaphore = &keyFillUnlockedSemaphore;
  app->keyFillLocked = &keyFillLocked;

  auto settings = CefSettings{};

  settings.multi_threaded_message_loop = true;
  settings.windowless_rendering_enabled = true;

  CefInitialize(mainArgs, settings, app, nullptr);

  //CefRunMessageLoop();

  for (;;) {
    keyFillUnlockedSemaphore.lock();
    keyFillLocked.reset();
    keyFill.render();
    keyFillLocked = keyFill.lock();
    keyFillLockedSemaphore.unlock();
  }

  CefShutdown();

  return EXIT_SUCCESS;
}

