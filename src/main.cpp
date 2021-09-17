
#include "include/base/cef_logging.h"
#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_library_loader.h"

#include "KeyFill.hpp"
#include "Light2D.hpp"
#include "WebServer.hpp"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <iostream>
#include <sstream>

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#include "include/cef_application_mac.h"
#endif

#ifdef WIN32
#include <windows.h>
#endif

using namespace std::literals;

constexpr auto index_html = R"(
  <label for="url">URL:</label>
  <input type="text" id="url">
  <button id="load">Load</button>
  <button id="force_load">Force Load</button>
  <button id="reset">Reset</button>
  <button id="shutdown">Shutdown</button>
  <script type="text/javascript">
    const url = document.getElementById("url");
    const load = document.getElementById("load");
    const force_load = document.getElementById("force_load");
    const shutdown = document.getElementById("shutdown");

    load.addEventListener
      ( "click"
      , async _ => {
          try {
            const response = await fetch
              ( "/load"
              , { method: "post"
                , body: url.value
                }
              );
          } catch(err) {
            console.error(`Error: ${err}`);
          }
        }
      );

    force_load.addEventListener
      ( "click"
      , async _ => {
          try {
            const response = await fetch
              ( "/force_load"
              , { method: "post"
                , body: url.value
                }
              );
          } catch(err) {
            console.error(`Error: ${err}`);
          }
        }
      );

    reset.addEventListener
      ( "click"
      , async _ => {
          try {
            const response = await fetch
              ( "/reset"
              , {method: "post"}
              );
          } catch(err) {
            console.error(`Error: ${err}`);
          }
        }
      );

    shutdown.addEventListener
      ( "click"
      , async _ => {
          try {
            const response = await fetch
              ( "/shutdown"
              , {method: "post"}
              );
          } catch(err) {
            console.error(`Error: ${err}`);
          }
        }
      );
  </script>
)"sv;

constexpr auto instructions_html1 = R"(
  <!DOCTYPE html>
  <html>
  <head>
    <title>keyfillwebview instructions</title>
    <meta charset="utf-8" />
    <meta http-equiv="Content-type" content="text/html; charset=utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <style type="text/css">
    body {
      margin: 0;
      padding: 0;
    }
    div {
      width: 1000px;
      margin: 5em auto;
      padding: 2em;
      color: #ffffff;
      background-color: #101070;
      font-size: xx-large;
      border-radius: 0.5em;
    }
    @media (max-width: 700px) {
      div {
        margin: 0 auto;
        width: auto;
      }
    }
    </style>
  </head>

  <body>
  <div>
    <h1>keyfillwebview</h1>
    <p>To load a page go to
http://)"sv;
constexpr auto instructions_html2 = R"(:8080</p>
  </div>
  </body>
  </html>
)"sv;

template <typename F>
class StringVisitor : public CefStringVisitor {
  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(StringVisitor);

  private:
    F f;

  public:
    StringVisitor(F f) : f{std::move(f)} {}

  private:
    void Visit(CefString const & str) override { f(str); }
};

struct HTTPHandler {
  CefRefPtr<CefBrowser>& browser;

  template <typename Callback>
  auto operator()(HTTP::Request req, Callback callback) {
    if (req.method == HTTP::Request::Verb::Post && req.target == "/load") {
      if (browser) {
        auto frame = browser->GetMainFrame();
        frame->GetSource
          ( new StringVisitor
            { [frame, req = std::move(req), callback = std::move(callback)] (CefString const & source) {
                auto source_stdstr = source.ToString();
                if(  source_stdstr.find("<title>keyfillwebview instructions</title>") != std::string::npos
                  || source_stdstr == "<html><head></head><body></body></html>"
                  )
                {
                  frame->LoadURL(req.body);
                  callback(HTTP::Response{req, HTTP::Response::Status::Ok, "", "text/html"});
                } else {
                  callback(HTTP::Response{req, HTTP::Response::Status::Forbidden, "Already in use", "text/html"});
                }
              }
            }
          );
      } else {
        callback(HTTP::Response{req, HTTP::Response::Status::ServiceUnavailable, "Browser not yet initialized", "text/html"});
      }
    } else if (req.method == HTTP::Request::Verb::Post && req.target == "/force_load") {
      if (browser) {
        browser->GetMainFrame()->LoadURL(req.body);
        callback(HTTP::Response{req, HTTP::Response::Status::Ok, "", "text/html"});
      } else {
        callback(HTTP::Response{req, HTTP::Response::Status::ServiceUnavailable, "Browser not yet initialized", "text/html"});
      }
    } else if (req.method == HTTP::Request::Verb::Post && req.target == "/is_active") {
      if (browser) {
        auto frame = browser->GetMainFrame();
        frame->GetSource
          ( new StringVisitor
            { [frame, req = std::move(req), callback = std::move(callback)] (CefString const & source) {
                auto source_stdstr = source.ToString();
                if(  source_stdstr.find("<title>keyfillwebview instructions</title>") != std::string::npos
                  || source_stdstr == "<html><head></head><body></body></html>"
                  )
                {
                  callback(HTTP::Response{req, HTTP::Response::Status::Ok, "false", "text/html"});
                } else {
                  callback(HTTP::Response{req, HTTP::Response::Status::Ok, "true", "text/html"});
                }
              }
            }
          );
      } else {
        callback(HTTP::Response{req, HTTP::Response::Status::ServiceUnavailable, "Browser not yet initialized", "text/html"});
      }
    } else if (req.method == HTTP::Request::Verb::Post && req.target == "/reset") {
      if (browser) {
        browser->GetMainFrame()->LoadURL("http://127.0.0.1:8080/instructions");
        callback(HTTP::Response{req, HTTP::Response::Status::Ok, "", "text/html"});
      } else {
        callback(HTTP::Response{req, HTTP::Response::Status::ServiceUnavailable, "Browser not yet initialized", "text/html"});
      }
    } else if (req.method == HTTP::Request::Verb::Post && req.target == "/shutdown") {
#ifdef WIN32
      auto process = HANDLE{};
      if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &process)) {
        return callback(HTTP::Response{req, HTTP::Response::Status::InternalServerError, "Could not get token", "text/html"});
      }

      auto privileges = TOKEN_PRIVILEGES{};
      LookupPrivilegeValue(nullptr, SE_SHUTDOWN_NAME, &privileges.Privileges[0].Luid);
      privileges.PrivilegeCount = 1;
      privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

      AdjustTokenPrivileges(process, false, &privileges, 0, nullptr, 0);

      if (GetLastError() != ERROR_SUCCESS) {
        return callback(HTTP::Response{req, HTTP::Response::Status::InternalServerError, "Could not adjust privileges", "text/html"});
      }

      if (!ExitWindowsEx(EWX_HYBRID_SHUTDOWN | EWX_SHUTDOWN, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_FLAG_PLANNED)) {
        return callback(HTTP::Response{req, HTTP::Response::Status::InternalServerError, "Could not shutdown", "text/html"});
      }

      return callback(HTTP::Response{req, HTTP::Response::Status::Ok, "", "text/html"});
#else
      return callback(HTTP::Response{req, HTTP::Response::Status::NotImplemented, "Shutdown not yet implemented for this OS", "text/html"});
#endif
    } else if (req.target == "/" || req.target == "/?") {
      callback(HTTP::Response{req, HTTP::Response::Status::Ok, std::string{index_html}, "text/html"});
    } else if (req.target == "/instructions") {
      callback(HTTP::Response{req, HTTP::Response::Status::Ok, std::string{instructions_html1} + asio::ip::host_name() + std::string{instructions_html2}, "text/html"});
    } else {
      callback(HTTP::Response{req, HTTP::Response::Status::NotFound, "404 : Not Found", "text/html"});
    }
  }
};

struct KeyFillEvent {
  void const * buffer;
  std::condition_variable& cv;
};

class Client : public CefClient, CefLifeSpanHandler, CefRenderHandler {
  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(Client);

  private:
    CefRefPtr<CefBrowser>& _browser;
    std::optional<KeyFill::Windows>& keyFill;

    std::condition_variable cv;
    std::mutex mutex;

  public:
    Client
      ( CefRefPtr<CefBrowser>& browser
      , std::optional<KeyFill::Windows>& keyFill
      )
      : _browser{browser}
      , keyFill{keyFill}
      {}

    // CefClient methods
    auto GetLifeSpanHandler() -> CefRefPtr<CefLifeSpanHandler> override { return this; }
    auto GetRenderHandler() -> CefRefPtr<CefRenderHandler> override { return this; }

    // CefLifeSpanHandler methods
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
      _browser = browser;
    }

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
*/

      if (keyFill) {
        {
          auto dst = keyFill->lock();
          std::memcpy(dst.pixels.get(), buffer, dst.pitch * 1080);
        }
        keyFill->render();
      }

/*
      if (keyFillEvent) {
        auto lock = std::unique_lock{mutex};
        keyFillEvent->push(buffer, cv);
        cv.wait(lock);
      }
*/
    }
};

class App : public CefApp, CefBrowserProcessHandler {
  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(App);

  private:
    CefRefPtr<CefBrowser>& _browser;
    std::optional<KeyFill::Windows>& keyFill;

  public:
    App
      ( CefRefPtr<CefBrowser>& browser
      , std::optional<KeyFill::Windows>& keyFill
      )
      : _browser{browser}
      , keyFill{keyFill}
      {}

    // CefApp methods
    auto GetBrowserProcessHandler() -> CefRefPtr<CefBrowserProcessHandler> override { return this; }

    // CefBrowserProcessHandler methods
    void OnContextInitialized() override {
      auto info = CefWindowInfo{};

      info.SetAsWindowless(0);

      auto settings = CefBrowserSettings{};

      settings.windowless_frame_rate = 25;

      auto client = CefRefPtr<Client>{new Client{_browser, keyFill}};

#ifdef WIN32
      info.SetAsPopup(nullptr, "Web View");
#endif

      CefBrowserHost::CreateBrowser(info, client, "http://127.0.0.1:8080/instructions", settings, nullptr, nullptr);
    }
};

#ifdef __APPLE__
// Provide the CefAppProtocol implementation required by CEF.
@interface Application : NSApplication <CefAppProtocol> {
 @private
  BOOL handlingSendEvent_;
}
@end

@implementation Application
- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  handlingSendEvent_ = handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event {
  CefScopedSendingEvent sendingEventScoper;
  [super sendEvent:event];
}

// |-terminate:| is the entry point for orderly "quit" operations in Cocoa. This
// includes the application menu's quit menu item and keyboard equivalent, the
// application's dock icon menu's quit menu item, "quit" (not "force quit") in
// the Activity Monitor, and quits triggered by user logout and system restart
// and shutdown.
//
// The default |-terminate:| implementation ends the process by calling exit(),
// and thus never leaves the main run loop. This is unsuitable for Chromium
// since Chromium depends on leaving the main run loop to perform an orderly
// shutdown. We support the normal |-terminate:| interface by overriding the
// default implementation. Our implementation, which is very specific to the
// needs of Chromium, works by asking the application delegate to terminate
// using its |-tryToTerminateApplication:| method.
//
// |-tryToTerminateApplication:| differs from the standard
// |-applicationShouldTerminate:| in that no special event loop is run in the
// case that immediate termination is not possible (e.g., if dialog boxes
// allowing the user to cancel have to be shown). Instead, this method tries to
// close all browsers by calling CloseBrowser(false) via
// ClientHandler::CloseAllBrowsers. Calling CloseBrowser will result in a call
// to ClientHandler::DoClose and execution of |-performClose:| on the NSWindow.
// DoClose sets a flag that is used to differentiate between new close events
// (e.g., user clicked the window close button) and in-progress close events
// (e.g., user approved the close window dialog). The NSWindowDelegate
// |-windowShouldClose:| method checks this flag and either calls
// CloseBrowser(false) in the case of a new close event or destructs the
// NSWindow in the case of an in-progress close event.
// ClientHandler::OnBeforeClose will be called after the CEF NSView hosted in
// the NSWindow is dealloc'ed.
//
// After the final browser window has closed ClientHandler::OnBeforeClose will
// begin actual tear-down of the application by calling CefQuitMessageLoop.
// This ends the NSApplication event loop and execution then returns to the
// main() function for cleanup before application termination.
//
// The standard |-applicationShouldTerminate:| is not supported, and code paths
// leading to it must be redirected.
- (void)terminate:(id)sender {
  if (SDL_GetEventState(SDL_QUIT) == SDL_ENABLE) {
    SDL_Event event;
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
  }
}
@end
#endif

#ifdef WIN32
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPTSTR lpCmdLine,
                      int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
  auto mainArgs = CefMainArgs{hInstance};
#else
auto main(int argc, char** argv) -> int {
  auto mainArgs = CefMainArgs{argc, argv};
#endif

#ifdef __APPLE__
  auto library_loader = CefScopedLibraryLoader{};
  if (!library_loader.LoadInMain()) {
    std::cerr << "Cannot load shared library\n";
    return EXIT_FAILURE;
  }
#endif

#ifdef __APPLE__
  [Application sharedApplication];
#endif

  auto browser = CefRefPtr<CefBrowser>{};
  auto keyFill = std::optional<KeyFill::Windows>{};

  auto app = CefRefPtr<App>{new App{browser, keyFill}};

  if (auto exitCode = CefExecuteProcess(mainArgs, nullptr, nullptr); exitCode >= 0) {
    return exitCode;
  }

  auto const address = boost::asio::ip::make_address("0.0.0.0");
  auto const port = static_cast<unsigned short>(8080);
  auto const noThreads = 4;

  auto server = WebServer<HTTPHandler>{HTTPHandler{browser}, boost::asio::ip::tcp::endpoint{address, port}, noThreads};

  auto l2DInit = L2D::L2DInit{};

  keyFill.emplace
    ( l2DInit
    , "Web View"
    , L2D::Rect{0, 0, 3840, 1080}
    , SDL_WINDOW_BORDERLESS
    );

  L2D::show_cursor(false);

  auto settings = CefSettings{};

  settings.windowless_rendering_enabled = true;

  CefInitialize(mainArgs, settings, app, nullptr);

#ifdef __APPLE__
  [[NSBundle mainBundle] loadNibNamed:@"MainMenu" owner:NSApp topLevelObjects:nil];
#endif

  auto running = true;
  while(running) {
    while (auto event = L2D::Events::poll()) {
      switch (event->type) {
        case SDL_QUIT:
          running = false;
          break;
        case SDL_KEYDOWN:
          if (!event->key.repeat) {
            if (event->key.keysym.sym == SDLK_r && (event->key.keysym.mod & KMOD_CTRL)) {
              if (browser) {
                browser->Reload();
              }
            }
          }
          break;
        default:
          break;
      }
    }
    CefDoMessageLoopWork();
    // Should use CefSettings.external_message_pump option and CefBrowserProcessHandler::OnScheduleMessagePumpWork()
  }

  browser = nullptr;

  CefShutdown();

  return EXIT_SUCCESS;
}
