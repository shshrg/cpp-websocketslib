#ifndef WEBSOCKETLIB_STATUS_CODES_H
#define WEBSOCKETLIB_STATUS_CODES_H

/**
 * @file status_codes.h
 * @brief Defines HTTP status codes as an enumeration.
 *
 * This enum provides named constants for all standard HTTP status codes
 * organized by category: informational, success, redirection, client error,
 * and server error.
 */

/**
 * @enum StatusCode
 * @brief Represents standard HTTP status codes.
 *
 * The naming follows the pattern <Description>_<Code> to avoid conflicts
 * with reserved keywords (e.g., OK_200 instead of OK).
 */
enum StatusCode {
    // Information responses (1xx)
    Continue_100 = 100,                   /**< 100 Continue */
    SwitchingProtocol_101 = 101,          /**< 101 Switching Protocols */
    Processing_102 = 102,                 /**< 102 Processing (WebDAV) */
    EarlyHints_103 = 103,                 /**< 103 Early Hints */

    // Successful responses (2xx)
    OK_200 = 200,                         /**< 200 OK */
    Created_201 = 201,                     /**< 201 Created */
    Accepted_202 = 202,                    /**< 202 Accepted */
    NonAuthoritativeInformation_203 = 203, /**< 203 Non-Authoritative Information */
    NoContent_204 = 204,                   /**< 204 No Content */
    ResetContent_205 = 205,                /**< 205 Reset Content */
    PartialContent_206 = 206,              /**< 206 Partial Content */
    MultiStatus_207 = 207,                 /**< 207 Multi-Status (WebDAV) */
    AlreadyReported_208 = 208,             /**< 208 Already Reported (WebDAV) */
    IMUsed_226 = 226,                      /**< 226 IM Used */

    // Redirection messages (3xx)
    MultipleChoices_300 = 300,             /**< 300 Multiple Choices */
    MovedPermanently_301 = 301,            /**< 301 Moved Permanently */
    Found_302 = 302,                        /**< 302 Found */
    SeeOther_303 = 303,                     /**< 303 See Other */
    NotModified_304 = 304,                  /**< 304 Not Modified */
    UseProxy_305 = 305,                     /**< 305 Use Proxy */
    unused_306 = 306,                       /**< 306 (Unused) */
    TemporaryRedirect_307 = 307,            /**< 307 Temporary Redirect */
    PermanentRedirect_308 = 308,            /**< 308 Permanent Redirect */

    // Client error responses (4xx)
    BadRequest_400 = 400,                   /**< 400 Bad Request */
    Unauthorized_401 = 401,                 /**< 401 Unauthorized */
    PaymentRequired_402 = 402,              /**< 402 Payment Required */
    Forbidden_403 = 403,                    /**< 403 Forbidden */
    NotFound_404 = 404,                     /**< 404 Not Found */
    MethodNotAllowed_405 = 405,             /**< 405 Method Not Allowed */
    NotAcceptable_406 = 406,                /**< 406 Not Acceptable */
    ProxyAuthenticationRequired_407 = 407,  /**< 407 Proxy Authentication Required */
    RequestTimeout_408 = 408,               /**< 408 Request Timeout */
    Conflict_409 = 409,                     /**< 409 Conflict */
    Gone_410 = 410,                         /**< 410 Gone */
    LengthRequired_411 = 411,               /**< 411 Length Required */
    PreconditionFailed_412 = 412,           /**< 412 Precondition Failed */
    PayloadTooLarge_413 = 413,              /**< 413 Payload Too Large */
    UriTooLong_414 = 414,                   /**< 414 URI Too Long */
    UnsupportedMediaType_415 = 415,         /**< 415 Unsupported Media Type */
    RangeNotSatisfiable_416 = 416,          /**< 416 Range Not Satisfiable */
    ExpectationFailed_417 = 417,            /**< 417 Expectation Failed */
    ImATeapot_418 = 418,                    /**< 418 I'm a teapot (RFC 2324) */
    MisdirectedRequest_421 = 421,           /**< 421 Misdirected Request */
    UnprocessableContent_422 = 422,         /**< 422 Unprocessable Content (WebDAV) */
    Locked_423 = 423,                       /**< 423 Locked (WebDAV) */
    FailedDependency_424 = 424,             /**< 424 Failed Dependency (WebDAV) */
    TooEarly_425 = 425,                     /**< 425 Too Early */
    UpgradeRequired_426 = 426,              /**< 426 Upgrade Required */
    PreconditionRequired_428 = 428,         /**< 428 Precondition Required */
    TooManyRequests_429 = 429,              /**< 429 Too Many Requests */
    RequestHeaderFieldsTooLarge_431 = 431,  /**< 431 Request Header Fields Too Large */
    UnavailableForLegalReasons_451 = 451,   /**< 451 Unavailable For Legal Reasons */

    // Server error responses (5xx)
    InternalServerError_500 = 500,          /**< 500 Internal Server Error */
    NotImplemented_501 = 501,               /**< 501 Not Implemented */
    BadGateway_502 = 502,                   /**< 502 Bad Gateway */
    ServiceUnavailable_503 = 503,           /**< 503 Service Unavailable */
    GatewayTimeout_504 = 504,               /**< 504 Gateway Timeout */
    HttpVersionNotSupported_505 = 505,      /**< 505 HTTP Version Not Supported */
    VariantAlsoNegotiates_506 = 506,        /**< 506 Variant Also Negotiates */
    InsufficientStorage_507 = 507,          /**< 507 Insufficient Storage (WebDAV) */
    LoopDetected_508 = 508,                 /**< 508 Loop Detected (WebDAV) */
    NotExtended_510 = 510,                  /**< 510 Not Extended */
    NetworkAuthenticationRequired_511 = 511 /**< 511 Network Authentication Required */
};

#endif // WEBSOCKETLIB_STATUS_CODES_H
