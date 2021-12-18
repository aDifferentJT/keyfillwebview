
#include "include/base/cef_logging.h"
#include "include/cef_app.h"
#include "include/cef_command_line.h"

#include "KeyFill.hpp"
#include "Light2D.hpp"
#include "WebServer.hpp"

#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#include <fmt/core.h>

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#include "include/cef_application_mac.h"
#include "include/wrapper/cef_library_loader.h"
#endif

#ifdef WIN32
#include <windows.h>
#endif

using namespace std::literals;

auto operator""_p(char const * data, std::size_t length) -> std::filesystem::path {
  return std::string_view{data, length};
}

#if defined(linux) || defined(__APPLE__)
static auto const config_dir = std::filesystem::path{std::getenv("HOME")} / ".config"_p / "keyfillwebview"_p;
#elif defined(WIN32)
static auto const config_dir = std::filesystem::path{std::getenv("APPDATA")} / "keyfillwebview"_p;
#else
#error Unknown platform
#endif

static auto const video_dir = config_dir / "videos"_p;

constexpr auto index_html1 = R"html(
  <h1>keyfillwebview Control Panel</h1>
  <h3>System</h3>
  <button onclick="fetch(&quot;/shutdown&quot;, {method: &quot;post&quot;})">Shutdown</button>
  <hr/>
  <h3>Loaded Page</h3>
  <label for="url">URL:</label>
  <input type="text" id="url">
  <button id="load">Load</button>
  <button id="force_load">Force Load</button>
  <button id="set_default">Set Default</button>
  <button onclick="fetch(&quot;/reset&quot;, {method: &quot;post&quot;})">Reset</button>
  <hr/>
  <h3>Visibility</h3>
  <button onclick="fetch(&quot;/show&quot;, {method: &quot;post&quot;})">Show</button>
  <button onclick="fetch(&quot;/clear&quot;, {method: &quot;post&quot;})">Clear</button>
  <button onclick="fetch(&quot;/black&quot;, {method: &quot;post&quot;})">Black</button>
  <hr/>
  <h3>Upload Video</h3>
  <input type="file" id="upload_video_file"/>
  <button id="upload_video">Upload</button>
  <hr/>
  <h3>Play Video</h3>
  <select name="select_video" id="select_video">
)html";
constexpr auto index_html2 = R"html(
  </select>
  <button id="load_video">Load</button>
  <button id="load_video_looping">Load (Looping)</button>
  <button onclick="fetch(&quot;/play_video&quot;,  {method: &quot;post&quot;})">Play </button>
  <button onclick="fetch(&quot;/pause_video&quot;, {method: &quot;post&quot;})">Pause</button>
  <script type="text/javascript">
    const url = document.getElementById("url");

    document.getElementById("load").onclick = async _ => {
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
    };

    document.getElementById("force_load").onclick = async _ => {
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
    };

    document.getElementById("set_default").onclick = async _ => {
      try {
        const response = await fetch
          ( "/set_default"
          , { method: "post"
            , body: url.value
            }
          );
      } catch(err) {
        console.error(`Error: ${err}`);
      }
    };

    document.getElementById("upload_video").onclick = async _ => {
      try {
        const file = document.getElementById("upload_video_file").files[0];
        const response = await fetch
          ( "/upload_video/" + file.name
          , { method: "post"
            , body: file
            }
          );
        window.location.reload(true);
      } catch(err) {
        console.error(`Error: ${err}`);
      }
    };

    document.getElementById("load_video").onclick = async _ => {
      try {
        const response = await fetch
          ( "/load_video"
          , { method: "post"
            , body: document.getElementById("select_video").value
            }
          );
      } catch(err) {
        console.error(`Error: ${err}`);
      }
    };

    document.getElementById("load_video_looping").onclick = async _ => {
      try {
        const response = await fetch
          ( "/load_video_looping"
          , { method: "post"
            , body: document.getElementById("select_video").value
            }
          );
      } catch(err) {
        console.error(`Error: ${err}`);
      }
    };
  </script>
)html"sv;

constexpr auto instructions_html = R"(
<!DOCTYPE html>
<html>
  <head>
    <title>keyfillwebview instructions</title>
    <meta charset="utf-8" />
    <meta http-equiv="Content-type" content="text/html; charset=utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <style type="text/css">
    body {{
      margin: 0;
      padding: 0;
    }}
    div {{
      width: 1000px;
      margin: 5em auto;
      padding: 2em;
      color: #ffffff;
      background-color: #101070;
      font-size: xx-large;
      border-radius: 0.5em;
    }}
    @media (max-width: 700px) {{
      div {{
        margin: 0 auto;
        width: auto;
      }}
    }}
    </style>
  </head>

  <body>
    <div>
      <h1>keyfillwebview</h1>
      <p>To load a page go to http://{}:8080</p>
    </div>
  </body>
</html>
)"sv;

constexpr auto video_player_html = R"html(
<!DOCTYPE html>
<html>
  <head>
    <title>keyfillwebview video player: {0}</title>
    <meta charset="utf-8" />
    <meta http-equiv="Content-type" content="text/html; charset=utf-8" />
  </head>

  <body style="margin: 0; padding: 0">
    <video id="video" width="100%" height="100%"/>
    <script>
      const params = new URLSearchParams(window.location.search);
      const video = document.getElementById("video");

      video.src = "/get_video/" + params.get("video");
      video.loop = params.get("looping") === "true";
      video.onended = () => window.location = params.get("returnto");

      fetch("/play_video", {method: "post"});

      document.onkeydown = e => {
        switch (e.key) {
          case "0":
            video.play();
            break;
          case "1":
            video.pause();
            break;
          default:
            console.log("Unknown key: " + e.key);
            break;
        }
      };
    </script>
  </body>
</html>
)html"sv;

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

enum class Mode { Show, Clear, Black };

struct HTTPHandler {
  CefRefPtr<CefBrowser>& browser;
  Mode& mode;

  template <typename Callback>
  auto operator()(HTTP::Request req, Callback callback) {
    if (req.method == HTTP::Request::Verb::Post && req.target == "/shutdown") {
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

      callback(HTTP::Response{req, HTTP::Response::Status::Ok, "", "text/html"});
#else
      callback(HTTP::Response{req, HTTP::Response::Status::NotImplemented, "Shutdown not yet implemented for this OS", "text/html"});
#endif
    } else if (req.method == HTTP::Request::Verb::Post && req.target == "/load") {
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
    } else if (req.method == HTTP::Request::Verb::Post && req.target == "/set_default") {
      auto defaultUrlFile = std::ofstream{SDL_GetPrefPath("nixCodeX", "keyfillwebview") + "defaultUrl"s};
      defaultUrlFile << req.body;
      callback(HTTP::Response{req, HTTP::Response::Status::Ok, "", "text/html"});
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
    } else if (req.method == HTTP::Request::Verb::Post && req.target == "/show") {
      mode = Mode::Show;
      browser->GetHost()->Invalidate(PET_VIEW);
    } else if (req.method == HTTP::Request::Verb::Post && req.target == "/clear") {
      mode = Mode::Clear;
      browser->GetHost()->Invalidate(PET_VIEW);
    } else if (req.method == HTTP::Request::Verb::Post && req.target == "/black") {
      mode = Mode::Black;
      browser->GetHost()->Invalidate(PET_VIEW);
    } else if (req.method == HTTP::Request::Verb::Post && req.target.find("/upload_video/") == 0) {
      auto const filename = req.target.substr(sizeof("/upload_video/") - 1);
      auto ec = std::error_code{};
      std::filesystem::create_directories(video_dir, ec);
      if (ec) {
        return callback(HTTP::Response{req, HTTP::Response::Status::InternalServerError, "Could not create video directory", "text/html"});
      }
      auto f = std::ofstream{video_dir / filename, std::ios::binary};
      f.write(req.body.data(), req.body.size());
      callback(HTTP::Response{req, HTTP::Response::Status::Ok, "", "text/html"});
    } else if (req.method == HTTP::Request::Verb::Post && req.target == "/load_video") {
      if (browser) {
        auto frame = browser->GetMainFrame();
        auto returnto = frame->GetURL().ToString();
        if (returnto.find("http://127.0.0.1:8080/video_player") == 0) {
          returnto = returnto.substr(returnto.find("&returnto=") + sizeof("&returnto=") - 1);
        }
        frame->LoadURL(fmt::format("http://127.0.0.1:8080/video_player?video={}&looping=false&returnto={}", req.body, returnto));
        callback(HTTP::Response{req, HTTP::Response::Status::Ok, "", "text/html"});
      } else {
        callback(HTTP::Response{req, HTTP::Response::Status::ServiceUnavailable, "Browser not yet initialized", "text/html"});
      }
    } else if (req.method == HTTP::Request::Verb::Post && req.target == "/load_video_looping") {
      if (browser) {
        auto frame = browser->GetMainFrame();
        auto returnto = frame->GetURL().ToString();
        if (returnto.find("http://127.0.0.1:8080/video_player") == 0) {
          returnto = returnto.substr(returnto.find("&returnto=") + sizeof("&returnto=") - 1);
        }
        frame->LoadURL(fmt::format("http://127.0.0.1:8080/video_player?video={}&looping=true&returnto={}", req.body, returnto));
        callback(HTTP::Response{req, HTTP::Response::Status::Ok, "", "text/html"});
      } else {
        callback(HTTP::Response{req, HTTP::Response::Status::ServiceUnavailable, "Browser not yet initialized", "text/html"});
      }
    } else if (req.method == HTTP::Request::Verb::Post && req.target == "/play_video") {
      if (browser) {
        //browser->GetMainFrame()->ExecuteJavaScript(R"(document.getElementsByTagName("video")[0].play())", "", 0);
        auto keyEvent = CefKeyEvent{};
        keyEvent.type = KEYEVENT_KEYDOWN;
        keyEvent.character = '0';
        keyEvent.windows_key_code = 0x30;
        browser->GetHost()->SendKeyEvent(keyEvent);
        callback(HTTP::Response{req, HTTP::Response::Status::Ok, "", "text/html"});
      } else {
        callback(HTTP::Response{req, HTTP::Response::Status::ServiceUnavailable, "Browser not yet initialized", "text/html"});
      }
    } else if (req.method == HTTP::Request::Verb::Post && req.target == "/pause_video") {
      if (browser) {
        browser->GetMainFrame()->ExecuteJavaScript(R"(document.getElementsByTagName("video")[0].pause())", "", 0);
        callback(HTTP::Response{req, HTTP::Response::Status::Ok, "", "text/html"});
      } else {
        callback(HTTP::Response{req, HTTP::Response::Status::ServiceUnavailable, "Browser not yet initialized", "text/html"});
      }
    } else if (req.target == "/" || req.target == "/?") {
      auto index_html = std::stringstream{};
      index_html << index_html1;

      auto ec1 = std::error_code{};
      if (std::filesystem::is_directory(video_dir, ec1)) {
        auto ec2 = std::error_code{};
        std::transform
          ( std::filesystem::directory_iterator{video_dir, ec2}
          , std::filesystem::directory_iterator{}
          , std::ostream_iterator<std::string>{index_html}
          , [] (std::filesystem::directory_entry video) {
              return fmt::format("<option value=\"{0}\">{0}</option>", video.path().filename().string());
            }
          );
        if (ec2) {
          return callback(HTTP::Response{req, HTTP::Response::Status::InternalServerError, "Could not access video directory", "text/html"});
        }
      }
  
      index_html << index_html2;

      callback(HTTP::Response{req, HTTP::Response::Status::Ok, std::move(index_html).str(), "text/html"});
    } else if (req.target == "/instructions") {
      callback(HTTP::Response{req, HTTP::Response::Status::Ok, fmt::format(instructions_html, asio::ip::host_name()), "text/html"});
    } else if (req.target.find("/video_player") == 0) {
      callback(HTTP::Response{req, HTTP::Response::Status::Ok, std::string{video_player_html}, "text/html"});
    } else if (req.target.find("/get_video/") == 0) {
      auto const filename = req.target.substr(sizeof("/get_video/") - 1);
      auto f = std::ifstream{video_dir / filename, std::ios::binary};
      auto video = std::string{};
      auto const batchSize = 1024;
      while (!f.eof()) {
        video.resize(video.size() + batchSize);
        f.read(&*video.end() - batchSize, batchSize);
        video.resize(video.size() - batchSize + f.gcount());
      }
      callback(HTTP::Response{req, HTTP::Response::Status::Ok, video, ""});
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
    Mode& mode;

    std::condition_variable cv;
    std::mutex mutex;

  public:
    Client
      ( CefRefPtr<CefBrowser>& browser
      , std::optional<KeyFill::Windows>& keyFill
      , Mode& mode
      )
      : _browser{browser}
      , keyFill{keyFill}
      , mode{mode}
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
        switch (mode) {
          case Mode::Show:
            {
              auto dst = keyFill->lock();
              std::memcpy(dst.pixels.get(), buffer, dst.pitch * 1080);
            }
            keyFill->render();
            break;
          case Mode::Clear:
            keyFill->renderClear();
            break;
          case Mode::Black:
            keyFill->renderBlack();
            break;
        }
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
    Mode& mode;

  public:
    App
      ( CefRefPtr<CefBrowser>& browser
      , std::optional<KeyFill::Windows>& keyFill
      , Mode& mode
      )
      : _browser{browser}
      , keyFill{keyFill}
      , mode{mode}
      {}

    // CefApp methods
    auto GetBrowserProcessHandler() -> CefRefPtr<CefBrowserProcessHandler> override { return this; }

    // CefBrowserProcessHandler methods
    void OnContextInitialized() override {
      auto info = CefWindowInfo{};

      info.SetAsWindowless(0);

      auto settings = CefBrowserSettings{};

      settings.windowless_frame_rate = 25;

      auto client = CefRefPtr<Client>{new Client{_browser, keyFill, mode}};

#ifdef WIN32
      info.SetAsPopup(nullptr, "Web View");
#endif

      auto url = "http://127.0.0.1:8080/instructions"s;

      auto defaultUrlFile = std::ifstream{SDL_GetPrefPath("nixCodeX", "keyfillwebview") + "defaultUrl"s};
      if (defaultUrlFile) {
        defaultUrlFile >> url;
      }

      CefBrowserHost::CreateBrowser(info, client, url, settings, nullptr, nullptr);
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
  auto mode = Mode::Show;

  auto app = CefRefPtr<App>{new App{browser, keyFill, mode}};

  if (auto exitCode = CefExecuteProcess(mainArgs, nullptr, nullptr); exitCode >= 0) {
    return exitCode;
  }

  auto const address = boost::asio::ip::make_address("0.0.0.0");
  auto const port = static_cast<unsigned short>(8080);
  auto const noThreads = 4;

  auto server = WebServer<HTTPHandler>{HTTPHandler{browser, mode}, boost::asio::ip::tcp::endpoint{address, port}, noThreads};

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

  auto refreshTimer = L2D::Timer
    { 2000
    , [&browser](uint32_t milliseconds) -> uint32_t {
        if (browser) {
          auto frame = browser->GetMainFrame();
          frame->GetSource
            ( new StringVisitor
              { [browser, frame] (CefString const & source) {
                  auto source_stdstr = source.ToString();
                  if (source_stdstr == "<html><head></head><body></body></html>") {
                    browser->Reload();
                  }
                }
              }
            );
        }
        return milliseconds;
      }
    };

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
