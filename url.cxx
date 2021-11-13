/*
url - A simple C++20 module for making HTTP requests. Version 0.0.3.

Written in 2021 by Petter Holmberg petter.holmberg@usa.net

Uses libcurl: https://curl.se/libcurl/

To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.

You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.

It is also recommended that you include a file called COPYING (or COPYING.txt) containing the CC0 legalcode as plain text.
*/

module;

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module url;

namespace {

struct curl_global
{
    ~curl_global()
    {
        ::curl_global_cleanup();
    }

    curl_global()
    {
        if (::curl_global_init(CURL_GLOBAL_ALL)) {
            throw std::runtime_error("curl_global::curl_global: curl_global_init failed");
        }
    }

    curl_global(curl_global const&) = delete;

    curl_global& operator=(curl_global const&) = delete;
};

curl_global curl_global_context;

struct curl_thread_local
{
    CURL* curl{};
    curl_slist* header_chunk{};
    std::vector<std::string> response_headers;

    ~curl_thread_local()
    {
        ::curl_slist_free_all(header_chunk);
        ::curl_easy_cleanup(curl);
    }

    curl_thread_local()
    {
        curl = ::curl_easy_init();
        if (!curl) {
            throw std::runtime_error("curl_thread_local::curl_thread_local: curl_easy_init failed");
        }
    }

    curl_thread_local(curl_thread_local const&) = delete;

    curl_thread_local& operator=(curl_thread_local const&) = delete;
};

thread_local curl_thread_local curl_thread_context;

struct user_data
{
    char* memory;
    size_t size{0};

    ~user_data()
    {
        ::free(memory);
    }

    user_data()
        : memory{reinterpret_cast<char*>(::malloc(1))}
    {
        if (!memory) throw std::runtime_error("user_data::user_data: malloc failed");
    }

    user_data(user_data const&) = delete;

    user_data& operator=(user_data const&) = delete;
};

auto
write_user_data(void* contents, size_t size, size_t nmemb, void* userp) -> size_t
{
    auto realsize{size * nmemb};
    auto mem{reinterpret_cast<user_data*>(userp)};

    auto ptr{reinterpret_cast<char*>(::realloc(mem->memory, mem->size + realsize + 1))};
    if (!ptr) {
        throw std::runtime_error("write_user_data: realloc failed");
    }

    mem->memory = ptr;
    ::memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

auto
header_callback(char* buffer, size_t size, size_t nitems, void*) -> size_t
{
    auto& response_headers = curl_thread_context.response_headers;
    response_headers.emplace_back(buffer, nitems);
    if (response_headers.back().ends_with("\r\n")) {
        response_headers.back().pop_back();
        response_headers.back().pop_back();
    }
    return size * nitems;
}

auto
encode_url(std::string_view url) -> std::string
{
    std::string url_str{url};
    {
        using url_handle_t = std::unique_ptr<CURLU, decltype(&::curl_url_cleanup)>;
        url_handle_t url_handle{::curl_url(), ::curl_url_cleanup};
        long const flags{CURLU_DEFAULT_SCHEME | CURLU_URLENCODE};
        auto curl_res{::curl_url_set(url_handle.get(), CURLUPART_URL, url_str.c_str(), flags)};
        if (curl_res == CURLUE_OK) {
            char* encoded_url;
            ::curl_url_get(url_handle.get(), CURLUPART_URL, &encoded_url, 0);
            url_str = encoded_url;
            ::curl_free(encoded_url);
        }
    }
    return url_str;
}

void
set_url_options(std::string_view url)
{
    auto& curl = curl_thread_context.curl;

    auto url_str{encode_url(url)};
    ::curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());
    ::curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    ::curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    ::curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
}

}

namespace url {

inline namespace v0 {

export using header_t = std::vector<std::string>;

export using form_t = std::vector<std::pair<std::string, std::string>>;

export struct response_t
{
    uint32_t status_code{0};
    header_t headers;
    std::string body;
    std::string encoding;
    std::string url;

    explicit constexpr
    operator bool() const noexcept
    {
        return status_code >= 100 && status_code < 400;
    }
};

export auto
operator<<(std::ostream& os, response_t const& response) -> std::ostream&
{
    os << response.body;
    return os;
}

static void
set_headers(header_t const& headers)
{
    auto& curl = curl_thread_context.curl;

    ::curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    if (!headers.empty()) {
        auto& header_chunk{curl_thread_context.header_chunk};
        for (auto const& header : headers) {
            header_chunk = ::curl_slist_append(header_chunk, header.c_str());
        }
        ::curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_chunk);
    }
}

static auto
request() -> response_t
{
    auto& curl = curl_thread_context.curl;

    ::curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_user_data);
    user_data chunk;
    ::curl_easy_setopt(curl, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&chunk));

    curl_thread_context.response_headers.clear();
    ::curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

    response_t response;
    auto curl_res{::curl_easy_perform(curl)};
    if (curl_res == CURLE_OK || curl_res == CURLE_HTTP_RETURNED_ERROR) {
        ::curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
        response.body.assign(chunk.memory, chunk.memory + chunk.size);

        response.headers = std::move(curl_thread_context.response_headers);

        if (
            auto content_type{
                std::ranges::find_if(
                    response.headers,
                    [](std::string const& header) {
                        std::string lower{header};
                        std::ranges::transform(lower, lower.begin(), ::tolower);
                        return lower.starts_with("content-type:");
                    }
                )
            };
            content_type != std::end(response.headers)
        ) {
            auto first{content_type->rfind("charset=")};
            if (first != std::string::npos) {
                auto last{content_type->find(';', first)};
                response.encoding = content_type->substr(first + 8, last);
            }
        }

        {
            char* effective_url{};
            ::curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
            if (effective_url) {
                response.url = effective_url;
            }
        }
    }

    return response;
}

static auto
update(std::string_view url, char const* method, std::string_view body, header_t const& headers) -> response_t
{
    set_url_options(url);

    auto& curl = curl_thread_context.curl;
    ::curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);

    set_headers(headers);

    if (!body.empty()) {
        std::string body_str{body};
        ::curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    }

    return request();
}

export auto
del(std::string_view url, std::string_view body = {}, header_t const& headers = {}) -> response_t
{
    return update(url, "DELETE", body, headers);
}

export auto
get(std::string_view url, header_t const& headers = {}) -> response_t
{
    set_url_options(url);
    set_headers(headers);
    return request();
}

export auto
head(std::string_view url, header_t const& headers = {}) -> response_t
{
    set_url_options(url);

    auto& curl = curl_thread_context.curl;
    ::curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

    set_headers(headers);
    return request();
}

export auto
patch(std::string_view url, std::string_view body, header_t const& headers = {}) -> response_t
{
    return update(url, "PATCH", body, headers);
}

export auto
post(std::string_view url, std::string_view body, header_t const& headers = {}) -> response_t
{
    set_url_options(url);
    set_headers(headers);

    auto& curl = curl_thread_context.curl;
    std::string body_str{body};
    ::curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());

    return request();
}

export auto
post(std::string_view url, form_t form, header_t const& headers = {}) -> response_t
{
    set_url_options(url);
    set_headers(headers);

    auto& curl = curl_thread_context.curl;
    using mime_t = std::unique_ptr<curl_mime, decltype(&::curl_mime_free)>;
    mime_t mime{::curl_mime_init(curl), ::curl_mime_free};
    for (const auto& field : form) {
        auto part{::curl_mime_addpart(mime.get())};
        ::curl_mime_name(part, field.first.c_str());
        ::curl_mime_data(part, field.second.c_str(), CURL_ZERO_TERMINATED);
    }
    ::curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime.get());

    return request();
}

export auto
put(std::string_view url, std::string_view body, header_t const& headers = {}) -> response_t
{
    return update(url, "PUT", body, headers);
}

}

}
