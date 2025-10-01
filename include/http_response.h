#ifndef WEBSOCKETLIB_HTTP_RESPONSE_H
#define WEBSOCKETLIB_HTTP_RESPONSE_H
#include "http_request.h"

enum StatusCode {
    // Information responses
    Continue_100 = 100,
    SwitchingProtocol_101 = 101,
    Processing_102 = 102,
    EarlyHints_103 = 103,

    // Successful responses
    OK_200 = 200,
    Created_201 = 201,
    Accepted_202 = 202,
    NonAuthoritativeInformation_203 = 203,
    NoContent_204 = 204,
    ResetContent_205 = 205,
    PartialContent_206 = 206,
    MultiStatus_207 = 207,
    AlreadyReported_208 = 208,
    IMUsed_226 = 226,

    // Redirection messages
    MultipleChoices_300 = 300,
    MovedPermanently_301 = 301,
    Found_302 = 302,
    SeeOther_303 = 303,
    NotModified_304 = 304,
    UseProxy_305 = 305,
    unused_306 = 306,
    TemporaryRedirect_307 = 307,
    PermanentRedirect_308 = 308,

    // client error responses
    BadRequest_400 = 400,
    Unauthorized_401 = 401,
    PaymentRequired_402 = 402,
    Forbidden_403 = 403,
    NotFound_404 = 404,
    MethodNotAllowed_405 = 405,
    NotAcceptable_406 = 406,
    ProxyAuthenticationRequired_407 = 407,
    RequestTimeout_408 = 408,
    Conflict_409 = 409,
    Gone_410 = 410,
    LengthRequired_411 = 411,
    PreconditionFailed_412 = 412,
    PayloadTooLarge_413 = 413,
    UriTooLong_414 = 414,
    UnsupportedMediaType_415 = 415,
    RangeNotSatisfiable_416 = 416,
    ExpectationFailed_417 = 417,
    ImATeapot_418 = 418,
    MisdirectedRequest_421 = 421,
    UnprocessableContent_422 = 422,
    Locked_423 = 423,
    FailedDependency_424 = 424,
    TooEarly_425 = 425,
    UpgradeRequired_426 = 426,
    PreconditionRequired_428 = 428,
    TooManyRequests_429 = 429,
    RequestHeaderFieldsTooLarge_431 = 431,
    UnavailableForLegalReasons_451 = 451,

    // Server error responses
    InternalServerError_500 = 500,
    NotImplemented_501 = 501,
    BadGateway_502 = 502,
    ServiceUnavailable_503 = 503,
    GatewayTimeout_504 = 504,
    HttpVersionNotSupported_505 = 505,
    VariantAlsoNegotiates_506 = 506,
    InsufficientStorage_507 = 507,
    LoopDetected_508 = 508,
    NotExtended_510 = 510,
    NetworkAuthenticationRequired_511 = 511,
  };

struct Response {
    std::string version;
    int status = -1;
    Headers headers;
    std::string body;
    std::string location;


    bool has_header(const std::string &key) const;
    std::string get_header_value(const std::string &key, const char *def = "",
                                   size_t id = 0) const;
    size_t get_header_value_u64(const std::string &key, size_t def = 0,
                                size_t id = 0) const;
    size_t get_header_value_count(const std::string &key) const;

    void set_redirect(const std::string &url, int status = StatusCode::Found_302);

    void set_content(const char *s, size_t n, const std::string &content_type);
    void set_content(const std::string &s, const std::string &content_type);
    void set_content(std::string &&s, const std::string &content_type);

    Response() = default;
    Response(const Response &) = default;
    Response &operator=(const Response &) = default;
    Response(Response &&) = default;
    Response &operator=(Response &&) = default;
    // Some destructor?

    static Response text(std::string s, int status=200, std::string_view charset="utf-8");
    static Response json(std::string s, int status=200);
    static Response html(std::string s, int status=200);

private:
    size_t content_length_ = 0;
    // Do we need it?
    // ContentProvider content_provider_;
    // ContentProviderResourceReleaser content_provider_resource_releaser_;
    // bool is_chunked_content_provider_ = false;
    // bool content_provider_success_ = false;
    // std::string file_content_path_;
    // std::string file_content_content_type_;
};


#endif //WEBSOCKETLIB_HTTP_RESPONSE_H