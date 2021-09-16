#ifndef WebServer_hpp
#define WebServer_hpp

#define BOOST_NO_EXCEPTIONS
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace asio = boost::asio;           // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace boost {
  void throw_exception(std::exception const & e) {
    std::cerr << e.what();
    std::terminate();
  }

#if (BOOST_VERSION >= 107300)
  void throw_exception(std::exception const & e, boost::source_location const & loc) {
    std::cerr << e.what() << "at " << loc;
    std::terminate();
  }
#endif
}

template <typename HTTPHandler>
class WebServer;

namespace HTTP {
  struct Request {
    public:
      enum class Verb : std::underlying_type_t<http::verb>
        { Get     = static_cast<std::underlying_type_t<http::verb>>(http::verb::get)
        , Head    = static_cast<std::underlying_type_t<http::verb>>(http::verb::head)
        , Post    = static_cast<std::underlying_type_t<http::verb>>(http::verb::post)
        , Put     = static_cast<std::underlying_type_t<http::verb>>(http::verb::put)
        , Delete  = static_cast<std::underlying_type_t<http::verb>>(http::verb::delete_)
        , Trace   = static_cast<std::underlying_type_t<http::verb>>(http::verb::trace)
        , Options = static_cast<std::underlying_type_t<http::verb>>(http::verb::options)
        , Connect = static_cast<std::underlying_type_t<http::verb>>(http::verb::connect)
        , Patch   = static_cast<std::underlying_type_t<http::verb>>(http::verb::patch)
        };

      bool keep_alive;
      Verb method;
      std::string target;
      unsigned int version;
      std::string body;

    private:
      Request(http::request<http::string_body>&& req)
        : keep_alive{req.keep_alive()}
        , method{static_cast<Verb>(req.method())}
        , target{req.target()}
        , version{req.version()}
        , body{std::move(req).body()}
        {}

      template <typename HTTPHandler>
      friend class ::WebServer;
  };

  struct Response {
    public:
      enum class Status : std::underlying_type_t<http::status>
        // 1xx Informational Response
        { Continue                      = static_cast<std::underlying_type_t<http::status>>(http::status::continue_)
        , SwitchingProtocols            = static_cast<std::underlying_type_t<http::status>>(http::status::switching_protocols)
        , Processing                    = static_cast<std::underlying_type_t<http::status>>(http::status::processing)
        // 2xx Success
        , Ok                            = static_cast<std::underlying_type_t<http::status>>(http::status::ok)
        , Created                       = static_cast<std::underlying_type_t<http::status>>(http::status::created)
        , Accepted                      = static_cast<std::underlying_type_t<http::status>>(http::status::accepted)
        , NonAuthoritative              = static_cast<std::underlying_type_t<http::status>>(http::status::non_authoritative_information)
        , NoContent                     = static_cast<std::underlying_type_t<http::status>>(http::status::no_content)
        , ResetContent                  = static_cast<std::underlying_type_t<http::status>>(http::status::reset_content)
        , PartialContent                = static_cast<std::underlying_type_t<http::status>>(http::status::partial_content)
        , MultiStatus                   = static_cast<std::underlying_type_t<http::status>>(http::status::multi_status)
        , AlreadyReported               = static_cast<std::underlying_type_t<http::status>>(http::status::already_reported)
        , IMUsed                        = static_cast<std::underlying_type_t<http::status>>(http::status::im_used)
        // 3xx Redirection
        , MultipleChoices               = static_cast<std::underlying_type_t<http::status>>(http::status::multiple_choices)
        , MovedPermanently              = static_cast<std::underlying_type_t<http::status>>(http::status::moved_permanently)
        , Found                         = static_cast<std::underlying_type_t<http::status>>(http::status::found)
        , SeeOther                      = static_cast<std::underlying_type_t<http::status>>(http::status::see_other)
        , NotModified                   = static_cast<std::underlying_type_t<http::status>>(http::status::not_modified)
        , UseProxy                      = static_cast<std::underlying_type_t<http::status>>(http::status::use_proxy)
        , TemporaryRedirect             = static_cast<std::underlying_type_t<http::status>>(http::status::temporary_redirect)
        , PermanentRedirect             = static_cast<std::underlying_type_t<http::status>>(http::status::permanent_redirect)
        // 4xx Client Error
        , BadRequest                    = static_cast<std::underlying_type_t<http::status>>(http::status::bad_request)
        , Unauthorised                  = static_cast<std::underlying_type_t<http::status>>(http::status::unauthorized)
        , PaymentRequired               = static_cast<std::underlying_type_t<http::status>>(http::status::payment_required)
        , Forbidden                     = static_cast<std::underlying_type_t<http::status>>(http::status::forbidden)
        , NotFound                      = static_cast<std::underlying_type_t<http::status>>(http::status::not_found)
        , MethodNotAllowed              = static_cast<std::underlying_type_t<http::status>>(http::status::method_not_allowed)
        , NotAcceptable                 = static_cast<std::underlying_type_t<http::status>>(http::status::not_acceptable)
        , ProxyAuthenticationRequired   = static_cast<std::underlying_type_t<http::status>>(http::status::proxy_authentication_required)
        , RequestTimeout                = static_cast<std::underlying_type_t<http::status>>(http::status::request_timeout)
        , Conflict                      = static_cast<std::underlying_type_t<http::status>>(http::status::conflict)
        , Gone                          = static_cast<std::underlying_type_t<http::status>>(http::status::gone)
        , LengthRequired                = static_cast<std::underlying_type_t<http::status>>(http::status::length_required)
        , PreconditionFailed            = static_cast<std::underlying_type_t<http::status>>(http::status::precondition_failed)
        , PayloadTooLarge               = static_cast<std::underlying_type_t<http::status>>(http::status::payload_too_large)
        , URITooLong                    = static_cast<std::underlying_type_t<http::status>>(http::status::uri_too_long)
        , UnsupportedMediaType          = static_cast<std::underlying_type_t<http::status>>(http::status::unsupported_media_type)
        , RangeNotSatisfiable           = static_cast<std::underlying_type_t<http::status>>(http::status::range_not_satisfiable)
        , ExpectationFailed             = static_cast<std::underlying_type_t<http::status>>(http::status::expectation_failed)
        , MisdirectedRequest            = static_cast<std::underlying_type_t<http::status>>(http::status::misdirected_request)
        , UnprocessableEntity           = static_cast<std::underlying_type_t<http::status>>(http::status::unprocessable_entity)
        , Locked                        = static_cast<std::underlying_type_t<http::status>>(http::status::locked)
        , FailedDependency              = static_cast<std::underlying_type_t<http::status>>(http::status::failed_dependency)
        , UpgradeRequired               = static_cast<std::underlying_type_t<http::status>>(http::status::upgrade_required)
        , PreconditionRequired          = static_cast<std::underlying_type_t<http::status>>(http::status::precondition_required)
        , TooManyRequests               = static_cast<std::underlying_type_t<http::status>>(http::status::too_many_requests)
        , RequestHeaderFieldsTooLarge   = static_cast<std::underlying_type_t<http::status>>(http::status::request_header_fields_too_large)
        , UnavailableForLegalReasons    = static_cast<std::underlying_type_t<http::status>>(http::status::unavailable_for_legal_reasons)
        // 5xx Server Error
        , InternalServerError           = static_cast<std::underlying_type_t<http::status>>(http::status::internal_server_error)
        , NotImplemented                = static_cast<std::underlying_type_t<http::status>>(http::status::not_implemented)
        , BadGateway                    = static_cast<std::underlying_type_t<http::status>>(http::status::bad_gateway)
        , ServiceUnavailable            = static_cast<std::underlying_type_t<http::status>>(http::status::service_unavailable)
        , GatewayTimeout                = static_cast<std::underlying_type_t<http::status>>(http::status::gateway_timeout)
        , HTTPVersionNotSupported       = static_cast<std::underlying_type_t<http::status>>(http::status::http_version_not_supported)
        , VariantAlsoNegotiates         = static_cast<std::underlying_type_t<http::status>>(http::status::variant_also_negotiates)
        , InsufficientStorage           = static_cast<std::underlying_type_t<http::status>>(http::status::insufficient_storage)
        , LoopDetected                  = static_cast<std::underlying_type_t<http::status>>(http::status::loop_detected)
        , NotExtended                   = static_cast<std::underlying_type_t<http::status>>(http::status::not_extended)
        , NetworkAuthenticationRequired = static_cast<std::underlying_type_t<http::status>>(http::status::network_authentication_required)
        };

      std::string body;
      bool keep_alive;
      std::string mime_type;
      Status status;
      unsigned int version;

      Response(Request const & req, Status status, std::string body, std::string mime_type)
        : body{std::move(body)}
        , keep_alive{req.keep_alive}
        , mime_type{std::move(mime_type)}
        , status{status}
        , version{req.version}
        {}

    private:
      auto beastResponse() -> http::response<http::string_body> {
        auto res = http::response<http::string_body>{static_cast<http::status>(status), version, body};
        res.keep_alive(keep_alive);
        res.set(http::field::content_type, mime_type);
        res.prepare_payload();
        return res;
      }

      template <typename HTTPHandler>
      friend class ::WebServer;
  };
}

template <typename HTTPHandler>
class WebServer {
  private:
    HTTPHandler httpHandler;

    asio::io_context ioc;

    tcp::endpoint endpoint;
    tcp::acceptor acceptor;

    // CLANG: This should be jthread but libc++ doesn't yet support jthread
    std::vector<std::thread> threads;

    void listen() {
      auto strand = std::make_shared<asio::strand<asio::io_context::executor_type>>(asio::make_strand(ioc));
      acceptor.async_accept
        ( *strand
        , [this, strand] (boost::system::error_code const & error, tcp::socket socket) mutable {
            auto stream = std::make_shared<beast::tcp_stream>(std::move(socket));
            asio::dispatch
              ( stream->get_executor()
              , [this, stream] {
                  auto buffer = std::make_shared<beast::flat_buffer>();
                  auto parser = std::make_shared<http::request_parser<http::string_body>>();
                  parser->body_limit(10000);

                  http::async_read
                    ( *stream
                    , *buffer
                    , *parser
                    , [this, stream, buffer, parser] (boost::system::error_code const & error, std::size_t bytes_transferred) {
                        if (error == http::error::end_of_stream) {
                          beast::error_code ec;
                          stream->socket().shutdown(tcp::socket::shutdown_send, ec);
                        } else {
                          httpHandler
                            ( HTTP::Request{parser->release()}
                            , [stream, buffer, parser] (HTTP::Response res) {
                                auto beastRes = std::make_shared<http::response<http::string_body>>(res.beastResponse());
                                http::async_write
                                  ( *stream
                                  , *beastRes
                                  , [stream, beastRes] (boost::system::error_code const & error, std::size_t bytes_transferred) {
                                      // Callback
                                    }
                                  );
                              }
                            );
                        }
                      }
                    );
                  listen();
                }
              );
          }
        );
    }

  public:
    WebServer
      ( HTTPHandler httpHandler
      , tcp::endpoint&& endpoint
      , int noThreads
      )
      : httpHandler{std::move(httpHandler)}
      , ioc{noThreads}
      , endpoint{std::move(endpoint)}
      , acceptor{ioc}
      {
      acceptor.open(endpoint.protocol());
      acceptor.set_option(asio::socket_base::reuse_address(true));
      acceptor.bind(endpoint);
      acceptor.listen(asio::socket_base::max_listen_connections);

      //asio::co_spawn(ioc, listen(), asio::detached);
      listen();

      // Run the I/O service on the requested number of threads
      threads.reserve(noThreads);
      for(auto i = 0; i < noThreads; ++i) {
        threads.emplace_back([&] { ioc.run(); });
      }
    }

    WebServer(WebServer const &) = delete;
    WebServer& operator=(WebServer const &) = delete;

    // TODO: These probably should exist
    WebServer(WebServer&&) = delete;
    WebServer& operator=(WebServer&&) = delete;

    ~WebServer() {
      ioc.stop();

      for (auto& t : threads) {
        t.join();
      }
    }
};

#endif
