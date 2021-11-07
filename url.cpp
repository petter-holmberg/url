/*
url - A simple C++20 module for making HTTP requests. Version 0.0.1.

Written in 2021 by Petter Holmberg petter.holmberg@usa.net

Uses libcurl: https://curl.se/libcurl/

To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.

You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.

It is also recommended that you include a file called COPYING (or COPYING.txt) containing the CC0 legalcode as plain text.
*/

module;

#include <curl/curl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
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
    CURL* curl{nullptr};
    curl_slist* header_chunk{nullptr};
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
        if (memory == nullptr) throw std::runtime_error("user_data::user_data: malloc failed");
    }

    user_data(user_data const&) = delete;

    user_data& operator=(user_data const&) = delete;
};

static auto
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

static auto
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

}

export namespace url {

inline namespace v0 {

using header_t = std::vector<std::string>;

struct response
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

auto
get(std::string_view url, header_t const& headers = {}) -> response
{
    auto& curl = curl_thread_context.curl;
    std::string url_str{url};
    ::curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());
    ::curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    ::curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    ::curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    if (!headers.empty()) {
        auto& header_chunk{curl_thread_context.header_chunk};
        for (const auto& header : headers) {
            header_chunk = ::curl_slist_append(header_chunk, header.c_str());
        }
        ::curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_chunk);
    }

    user_data chunk;
    ::curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_user_data);
    ::curl_easy_setopt(curl, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&chunk));

    curl_thread_context.response_headers.clear();
    ::curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

    response res;
    auto curl_res{::curl_easy_perform(curl)};
    if (curl_res == CURLE_OK || curl_res == CURLE_HTTP_RETURNED_ERROR) {
        ::curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res.status_code);
        res.body.assign(chunk.memory, chunk.memory + chunk.size);

        res.headers = std::move(curl_thread_context.response_headers);

        if (
            auto content_type{
                std::ranges::find_if(
                    res.headers,
                    [](const std::string& header) { return header.starts_with("content-type:"); }
                )
            };
            content_type != std::end(res.headers)
        ) {
            auto first{content_type->rfind("charset=")};
            if (first != std::string::npos) {
                auto last{content_type->find(';', first)};
                res.encoding = content_type->substr(first + 8, last);
            }
        }

        {
            char* effective_url{nullptr};
            ::curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
            if (effective_url) {
                res.url = effective_url;
            }
        }
    }

    return res;
}

}

}
